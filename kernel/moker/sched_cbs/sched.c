// SPDX-License-Identifier: GPL-2.0

#include <linux/math64.h>
#include "cbs_rq.h"
#include "cbs_task.h"
#include "../../sched/sched.h"
#ifdef CONFIG_MOKER_TRACING
#include "../trace/trace.h"
#endif


static void update_curr_cbs(struct rq *rq);


/**
 * task_has_cbs_policy - check if task has SCHED_CBS policy
 * @p: task pointer
 *
 * Return: non-zero if @p is scheduled under SCHED_CBS.
 */
static inline int task_has_cbs_policy(struct task_struct *p)
{
	return p->policy == SCHED_CBS;
}

/**
 * cbs_task_of - obtain the task_struct containing a CBS entity
 * @cbs_se: pointer to the CBS scheduling entity
 *
 * Return: pointer to the containing `task_struct`.
 */
static inline struct task_struct *cbs_task_of(struct sched_cbs_entity *cbs_se)
{
	return container_of(cbs_se, struct task_struct, cbs);
}

/**
 * sched_cbs_entity_is_hard - check whether an entity is hard real-time
 * @p: CBS scheduling entity
 *
 * Return: non-zero if the entity is configured as hard (no budget).
 */
static inline int sched_cbs_entity_is_hard(struct sched_cbs_entity *p)
{
	return p->server.capacity == 0; /* TODO: Review this */
}

/**
 * sched_cbs_entity_update - update entity budget and slice start
 * @p: CBS scheduling entity
 * @now: current time in nanoseconds
 *
 * Decrease the entity's remaining budget by the elapsed time since
 * @p->slice_start (saturating at 0) and set @p->slice_start to @now.
 */
static inline void sched_cbs_entity_update(struct sched_cbs_entity *p, u64 now)
{
	u64 delta;

	delta = now - p->slice_start;

	if(p->server.remaining_budget > delta) {
		p->server.remaining_budget -= delta;
	} else {
		p->server.remaining_budget = 0;
	}

	p->slice_start = now;
}

/**
 * sched_cbs_entity_hr_replenish_callback - replenish timer callback
 * @timer: pointer to the entity's replenish timer
 *
 * Replenishes the server budget, postpones the deadline as needed,
 * requeues the task and potentially requests a reschedule. Returns
 * HRTIMER_RESTART when the caller re-arms the replenish timer.
 */
static enum hrtimer_restart sched_cbs_entity_hr_replenish_callback(struct hrtimer *timer)
{
	struct rq *rq;
	struct rq_flags rflags;
	struct cbs_rq *cbs_rq;
	struct task_struct *p;
	struct sched_cbs_entity *cbs_se;
	struct sched_cbs_entity *leftmost_se;
	struct rb_node *leftmost;
	enum hrtimer_restart ret;
	u64 now;

	ret = HRTIMER_NORESTART;

	cbs_se = container_of(container_of(timer,
					   struct sched_cbs_entity_server,
					   hr_replenish),
			      struct sched_cbs_entity,
			      server);
	p = cbs_task_of(cbs_se);
	rq = task_rq_lock(p, &rflags); /* Grabs the rq and locks it  */
	cbs_rq = &rq->cbs;

	update_rq_clock(rq); /* Requires rq_lock */

	raw_spin_lock(&cbs_rq->lock);

	now = ktime_get_ns();

	// 1. replenish server's remaining budget
	cbs_se->server.remaining_budget = cbs_se->server.capacity;

#ifdef CONFIG_MOKER_TRACING
        moker_trace(BUDGET_REPLEN_SOFT, p, cbs_se->id);
#endif

	/*
	 * 2. postpone deadline
	 * This will effectively re-arm the deadline timer as well,
	 * since in its callback we compare the timer's expiry to task's deadline.
	 */
	cbs_se->deadline = cbs_se->deadline + cbs_se->period;

	// 3. update slice_start
	cbs_se->slice_start = now;

	if (!cbs_se->on_rq)
		goto unlock;

	// 4. requeue task
	rb_erase_cached(&cbs_se->rb_node, &cbs_rq->tasks_tree);
	RB_CLEAR_NODE(&cbs_se->rb_node);
	rb_add_cached(&cbs_se->rb_node, &cbs_rq->tasks_tree, cbs_rq_less);

#ifdef CONFIG_MOKER_TRACING
	moker_trace(REQUEUE_RQ, p, cbs_se->id);
#endif

	// 4. resched_curr if needed
	if(!task_has_cbs_policy(rq->curr)) {
		resched_curr(rq);
		goto unlock;
	}


	leftmost = rb_first_cached(&cbs_rq->tasks_tree);
	if(!leftmost)
		goto unlock;

	leftmost_se = container_of(leftmost, struct sched_cbs_entity, rb_node);
	if(cbs_task_of(leftmost_se) != rq->curr) {
		resched_curr(rq);
		goto unlock;
	}

	if(p == rq->curr) {
		/* hrtimer_set_expires expects an absolute expiry value */
		hrtimer_set_expires(timer,
				    ns_to_ktime(now + cbs_se->server.remaining_budget));
		ret = HRTIMER_RESTART;
	}

unlock:
	trace_printk("[id:%d] Budget Replenished [Budget=%llu] [D=%llu]\n",
		     cbs_se->id,
		     (unsigned long long)cbs_se->server.remaining_budget,
		     (unsigned long long)cbs_se->deadline);

	raw_spin_unlock(&cbs_rq->lock);

	task_rq_unlock(rq, p, &rflags);

	return ret;
}

/**
 * sched_cbs_entity_hr_replenish_setup - initialize the replenish hrtimer
 * @p: CBS scheduling entity
 *
 * Prepares the relative replenish timer used to track budget consumption.
 */
static void sched_cbs_entity_hr_replenish_setup(struct sched_cbs_entity *p)
{
	struct hrtimer *timer;
	clockid_t clock_id;
	enum hrtimer_mode mode;

	timer = &p->server.hr_replenish;
	clock_id = CLOCK_MONOTONIC;
	mode = HRTIMER_MODE_REL_HARD;

	hrtimer_setup(timer,
		      sched_cbs_entity_hr_replenish_callback,
		      clock_id,
		      mode);
}

/**
 * sched_cbs_entity_hr_replenish_arm - arm the replenish timer
 * @p: CBS scheduling entity
 *
 * Starts the replenish timer relative to the entity's remaining budget.
 */
static void sched_cbs_entity_hr_replenish_arm(struct sched_cbs_entity *p)
{
	struct hrtimer *timer;
	ktime_t fire_at;
	enum hrtimer_mode mode;

	if(p->server.remaining_budget <= 0) {
		trace_printk("[id:%d] Replenish is <=0\n", p->id);
		return;
	}

	timer = &p->server.hr_replenish;
	fire_at = ns_to_ktime(p->server.remaining_budget);
	mode = HRTIMER_MODE_REL_HARD;

	hrtimer_start(timer, fire_at, mode);
}

/**
 * sched_cbs_entity_hr_replenish_disarm - cancel the replenish timer
 * @p: CBS scheduling entity
 *
 * Return: non-zero if the timer was cancelled, zero otherwise.
 */
static int sched_cbs_entity_hr_replenish_disarm(struct sched_cbs_entity *p)
{
	struct hrtimer *timer;
	int ret;

	timer = &p->server.hr_replenish;

	ret = hrtimer_try_to_cancel(timer);

	return ret;
}

/**
 * sched_cbs_entity_hr_timers_setup - setup all hr timers for an entity
 * @p: CBS scheduling entity
 *
 * Configures deadline and (for soft servers) replenish timers.
 */
static void sched_cbs_entity_hr_timers_setup(struct sched_cbs_entity *p)
{
	if(sched_cbs_entity_is_hard(p))
		return;

	sched_cbs_entity_hr_replenish_setup(p);
}

/**
 * sched_cbs_entity_calc_deadline - compute or postpone entity deadline
 * @p: CBS scheduling entity
 * @arrival: arrival time in ns
 *
 * Calculate the entity's deadline based on arrival time, server state
 * and CBS rules. May reset remaining budget when producing a new deadline.
 */
static void sched_cbs_entity_calc_deadline(struct sched_cbs_entity *p, u64 arrival)
{
	int is_first;
	s64 diff;
	u64 lhs;
	u64 rhs;

	if(sched_cbs_entity_is_hard(p)) {
		p->deadline = arrival + p->period; /* Di = Ti */
		return;
	}

	/* Server was idle */
	is_first = p->server.first;
	if(is_first) {
		p->deadline = arrival + p->period;
		p->server.remaining_budget = p->server.capacity;
		p->server.first = 0;
		return;
	}

	/*
	 * Check if we need to postpone deadline in two stages
	 * 1. if the distance between current deadline and arrival is negative
	 *    we know that we need to generate a new deadline
	 */

	diff = (s64)(p->deadline - arrival);
	if (diff <= 0) {
		p->deadline = arrival + p->period;
		p->server.remaining_budget = p->server.capacity;
		return;
	}

	/*
	 * Caution: cs, Ts, di, ri, and Qs are all 64 bit values
	 *          with nanosecond precision.
	 *          Operating over these may overflow.
	 *          'mul_u64_u64_div_u64' (see include/linux/math64.h)
	 *          is a helper macro that does full 64 bit math safely.
	 */

	/* 2. if (cs >= (di - ri) * Qs/Ts), then generate new deadline */
	lhs = p->server.remaining_budget;
	rhs = mul_u64_u64_div_u64((u64)diff, p->server.capacity, p->period);
	if (lhs >= rhs) {
		p->deadline = arrival + p->period;
		p->server.remaining_budget = p->server.capacity;
	}
}

/**
 * enqueue_task_cbs - enqueue a task into the CBS runqueue
 * @rq: runqueue pointer
 * @p: task to enqueue
 * @flags: enqueue flags (unused)
 *
 * Calculates the task deadline, inserts it into the CBS red-black tree,
 * sets up the hrtimers and marks the task as runnable on the CBS rq.
 */
static void enqueue_task_cbs(struct rq *rq, struct task_struct *p, int flags)
{
        struct sched_cbs_entity *cbs_se;
        struct cbs_rq *cbs_rq;
        u64 now;

	if(!task_has_cbs_policy(p))
		return;

	raw_spin_lock(&rq->cbs.lock);

	cbs_se = &p->cbs;
	cbs_rq = &rq->cbs;

	if(cbs_se->on_rq) {
		raw_spin_unlock(&rq->cbs.lock);
		return;
	}

	now = ktime_get_ns();

	// 1. calculate deadline
	sched_cbs_entity_calc_deadline(cbs_se, now);

	// 2. insert in tree
	rb_add_cached(&cbs_se->rb_node, &cbs_rq->tasks_tree, cbs_rq_less);

	// 3. setup deadline & replenish timers
	sched_cbs_entity_hr_timers_setup(cbs_se);

	// 4. mark task as part of the rq
        cbs_se->on_rq = 1;
        add_nr_running(rq, 1);

	trace_printk("[id:%d][hard:%d][Ci:%llu][Ti:%llu][Di:%llu][Bi:%llu]\n",
		     cbs_se->id,
		     sched_cbs_entity_is_hard(cbs_se),
		     (unsigned long long)cbs_se->runtime,
		     (unsigned long long)cbs_se->period,
		     (unsigned long long)cbs_se->deadline,
		     (unsigned long long)cbs_se->server.remaining_budget);

	raw_spin_unlock(&rq->cbs.lock);

#ifdef CONFIG_MOKER_TRACING
        moker_trace(ENQUEUE_RQ, p, cbs_se->id);
#endif
}

/**
 * dequeue_task_cbs - remove a task from the CBS runqueue
 * @rq: runqueue pointer
 * @p: task to dequeue
 * @flags: dequeue flags (unused)
 *
 * Disarms the deadline timer, removes the task from the tree, updates
 * remaining budget if it was on CPU, and marks the task as not on rq.
 *
 * Return: true if the task was on the runqueue, false otherwise.
 */
static bool dequeue_task_cbs(struct rq *rq, struct task_struct *p, int flags)
{
        struct sched_cbs_entity *cbs_se;
        struct cbs_rq           *cbs_rq;
	u64 now;

	raw_spin_lock(&rq->cbs.lock);

	cbs_se = &p->cbs;
	cbs_rq = &rq->cbs;

	if(!cbs_se->on_rq) {
		raw_spin_unlock(&rq->cbs.lock);
		return false;
	}

	// 1. erase task from rq
	rb_erase_cached(&cbs_se->rb_node, &cbs_rq->tasks_tree);
	RB_CLEAR_NODE(&cbs_se->rb_node);

	now = ktime_get_ns();

	// 2. update remaining_budget
	if(p == rq->curr)
		sched_cbs_entity_update(cbs_se, now);

	// 4. mark task as not part of the rq
	cbs_se->on_rq = 0;
	sub_nr_running(rq, 1);

	raw_spin_unlock(&rq->cbs.lock);

#ifdef CONFIG_MOKER_TRACING
        moker_trace(DEQUEUE_RQ, p, cbs_se->id);
#endif
        return true;
}

/**
 * wakeup_preempt_cbs - decide whether a waking CBS task should preempt
 * @rq: runqueue pointer
 * @p: waking task
 * @flags: wakeup flags (unused)
 *
 * If the current task is CBS and the waking task has an earlier deadline
 * request a reschedule to allow preemption.
 */
static void wakeup_preempt_cbs(struct rq *rq, struct task_struct *p, int flags)
{
        struct task_struct *curr;

	curr = rq->curr;

	if(!task_has_cbs_policy(curr)) {
		resched_curr(rq);
		return;
	}

	if(!task_has_cbs_policy(p))
		return;

        if (p->cbs.deadline < curr->cbs.deadline) {
#ifdef CONFIG_MOKER_TRACING
		moker_trace(PREEMPT_RQ, p, curr->cbs.id);
#endif
                resched_curr(rq);
	}
}

/**
 * pick_task_cbs - pick the next CBS task to run
 * @rq: runqueue pointer
 * @rf: rq flags (unused)
 *
 * Returns the task_struct for the leftmost (earliest-deadline) entity
 * in the CBS runqueue, or NULL if none.
 */
static struct task_struct *pick_task_cbs(struct rq *rq, struct rq_flags *rf)
{
	struct task_struct *picked;
	struct rb_node *leftmost;

	raw_spin_lock(&rq->cbs.lock);

	leftmost = rb_first_cached(&rq->cbs.tasks_tree);
	if (!leftmost) {
		picked = NULL;
		goto unlock;
	}

	picked = cbs_task_of(container_of(leftmost,
					  struct sched_cbs_entity,
					  rb_node));

unlock:
	raw_spin_unlock(&rq->cbs.lock);

	return picked;
}

/*
 * @p: task currently on CPU
 * @next: task to run next on CPU
 */
/**
 * put_prev_task_cbs - bookkeeping when a CBS task is replaced on CPU
 * @rq: runqueue pointer
 * @p: task being removed from CPU
 * @next: task that will run next (unused)
 *
 * Updates the entity's remaining budget, and disarms the replenish timer
 * for soft servers.
 */
static void put_prev_task_cbs(struct rq *rq, struct task_struct *p,
				  struct task_struct *next)
{
	struct sched_cbs_entity *cbs_se;
	u64 now;
	int is_disarmed;

	cbs_se = &p->cbs;

	if (!task_has_cbs_policy(p))
		return;

	if(sched_cbs_entity_is_hard(cbs_se))
		return;

	now = ktime_get_ns();

	// 1. update server's remaining_budget
	sched_cbs_entity_update(cbs_se, now);

	// 2. cancel the budget tracking timer
	is_disarmed = sched_cbs_entity_hr_replenish_disarm(cbs_se);
	trace_printk("[id:%d] Replen disarmed [status:%d]\n",
		     cbs_se->id, is_disarmed);

#ifdef CONFIG_MOKER_TRACING
        moker_trace(DISARM_REPLEN_SOFT, p, cbs_se->id);
#endif
}

/**
 * set_next_task_cbs - prepare a CBS task when it is chosen to run
 * @rq: runqueue pointer
 * @p: task to run next
 * @first: true if this is the task's first slice
 *
 * Records the slice start time and arms the replenish timer for soft
 * servers as needed.
 */
static void set_next_task_cbs(struct rq *rq, struct task_struct *p, bool first)
{
	struct sched_cbs_entity *cbs_se;
	u64 now;

	cbs_se = &p->cbs;
	now = ktime_get_ns();

	// 1. time the task's execution start
	cbs_se->slice_start = now;

	if(sched_cbs_entity_is_hard(cbs_se))
		return;

	// 2. arm replenish to relative time from server.remaining_budget
	if(!hrtimer_active(&cbs_se->server.hr_replenish)) {
		sched_cbs_entity_hr_replenish_arm(cbs_se);
#ifdef CONFIG_MOKER_TRACING
		moker_trace(ARM_REPLEN_SOFT, p, cbs_se->id);
#endif
	}
}

/**
 * select_task_rq_cbs - choose a CPU for a CBS task
 * @p: task to place
 * @cpu: preferred cpu
 * @flags: selection flags (unused)
 *
 * Currently simply returns the preferred cpu.
 */
static int select_task_rq_cbs(struct task_struct *p, int cpu, int flags)
{
	return cpu;
}

/**
 * task_tick_cbs - per-tick handler for CBS tasks
 * @rq: runqueue pointer
 * @p: currently running task
 * @queued: queued flag (unused)
 *
 * Updates the current entity's budget and triggers reschedule when needed.
 */
static void task_tick_cbs(struct rq *rq, struct task_struct *p, int queued)
{
	update_curr_cbs(rq);
}

/**
 * prio_changed_cbs - handle priority change for CBS tasks
 * @rq: runqueue pointer
 * @p: affected task
 * @oldprio: previous priority (unused)
 *
 * Currently a no-op; present to satisfy scheduler interface.
 */
static void prio_changed_cbs(struct rq *rq, struct task_struct *p, u64 oldprio)
{}

/**
 * switched_to_cbs - handle task switch to CBS policy
 * @rq: runqueue pointer
 * @task: task switched to CBS
 *
 * Currently a no-op; present to satisfy scheduler interface.
 */
static void switched_to_cbs(struct rq *rq, struct task_struct *task)
{}

/**
 * update_curr_cbs - update the currently running CBS entity
 * @rq: runqueue pointer
 *
 * Updates the running entity's remaining budget based on elapsed time
 * and requests a reschedule if the budget is exhausted.
 */
static void update_curr_cbs(struct rq *rq)
{
	struct task_struct *curr = rq->curr;
	struct sched_cbs_entity *cbs_se;
	u64 now;

	if (!task_has_cbs_policy(curr))
		return;

	cbs_se = &curr->cbs;

	if (sched_cbs_entity_is_hard(cbs_se))
		return;

	now = ktime_get_ns();
	sched_cbs_entity_update(cbs_se, now);

	if (cbs_se->server.remaining_budget == 0)
		resched_curr(rq);
}

DEFINE_SCHED_CLASS(cbs) = {
        .queue_mask        = 8,

        .enqueue_task      = enqueue_task_cbs,
        .dequeue_task      = dequeue_task_cbs,

        .wakeup_preempt    = wakeup_preempt_cbs,

        .pick_task         = pick_task_cbs,
        .put_prev_task     = put_prev_task_cbs,
        .set_next_task     = set_next_task_cbs,

        .select_task_rq    = select_task_rq_cbs,
	.set_cpus_allowed  = set_cpus_allowed_common,

	.task_tick         = task_tick_cbs,

	.prio_changed      = prio_changed_cbs,
        .switched_to       = switched_to_cbs,

        .update_curr       = update_curr_cbs,
};
