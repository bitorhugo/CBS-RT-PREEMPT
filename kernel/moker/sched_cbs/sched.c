// SPDX-License-Identifier: GPL-2.0

#include <linux/math64.h>
#include "cbs_rq.h"
#include "cbs_task.h"
#include "../../sched/sched.h"
#ifdef CONFIG_MOKER_TRACING
#include "../trace/trace.h"
#endif


static void update_curr_cbs(struct rq *rq);


static inline int task_has_cbs_policy(struct task_struct *p)
{
	return p->policy == SCHED_CBS;
}

static inline struct task_struct *cbs_task_of(struct sched_cbs_entity *cbs_se)
{
	return container_of(cbs_se, struct task_struct, cbs);
}

static inline int sched_cbs_entity_is_hard(struct sched_cbs_entity *p)
{
	return p->server.capacity == 0; /* TODO: Review this */
}

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


static enum hrtimer_restart sched_cbs_entity_hr_deadline_callback(struct hrtimer *timer)
{
	struct rq *rq;
	struct rq_flags rflags;
	struct task_struct *p;
	struct sched_cbs_entity *cbs_se;
	u64 expiry;

	cbs_se = container_of(timer, struct sched_cbs_entity, hr_deadline);
	p = cbs_task_of(cbs_se);

	rq = task_rq_lock(p, &rflags); /* Grab the rq this task belongs to */

	update_rq_clock(rq); /* Requires rq_lock */

	if(sched_cbs_entity_is_hard(cbs_se)) {
		/*
		 * We don't want to do anything about hard tasks except log something.
		 * We assume schedulability analysis is a pre-condition.
		 */
		trace_printk("[id:%d] Hard Deadline not met\n", cbs_se->id);

		task_rq_unlock(rq, p, &rflags);

		return HRTIMER_NORESTART;
	}

	expiry = ktime_to_ns(hrtimer_get_expires(timer));
	if(cbs_se->deadline <= expiry) {
		trace_printk("[id:%d] Soft Deadline not updated\n", cbs_se->id);

		task_rq_unlock(rq, p, &rflags);

		return HRTIMER_NORESTART;
	}

	hrtimer_set_expires(timer, ns_to_ktime(cbs_se->deadline));
	trace_printk("[id:%d] Deadline updated [D=%llu]\n",
		     cbs_se->id,
		     (unsigned long long)cbs_se->deadline);

	task_rq_unlock(rq, p, &rflags);

	return HRTIMER_RESTART;
}


static void sched_cbs_entity_hr_deadline_setup(struct sched_cbs_entity *p)
{
	struct hrtimer *timer;
	clockid_t clock_id;
	enum hrtimer_mode mode;

	timer = &p->hr_deadline;
	clock_id = CLOCK_MONOTONIC;
	mode = HRTIMER_MODE_ABS_HARD;

	hrtimer_setup(timer,
		      sched_cbs_entity_hr_deadline_callback,
		      clock_id,
		      mode);
}


static void sched_cbs_entity_hr_deadline_arm(struct sched_cbs_entity *p)
{
	struct hrtimer *timer;
	ktime_t deadline;
	enum hrtimer_mode mode;

	timer = &p->hr_deadline;
	deadline = ns_to_ktime(p->deadline);
	mode = HRTIMER_MODE_ABS_HARD;

	hrtimer_start(timer, deadline, mode);
}


static int sched_cbs_entity_hr_deadline_disarm(struct sched_cbs_entity *p)
{
	struct hrtimer *timer;
	int ret;

	timer = &p->hr_deadline;

	ret = hrtimer_try_to_cancel(timer);

	return ret;
}


static enum hrtimer_restart sched_cbs_entity_hr_replenish_callback(struct hrtimer *timer)
{
	struct rq *rq;
	struct rq_flags rflags;
	struct cbs_rq *cbs_rq;
	struct task_struct *p;
	struct sched_cbs_entity *cbs_se;
	u64 now;

	cbs_se = container_of(container_of(timer,
					   struct sched_cbs_entity_server,
					   hr_replenish),
			      struct sched_cbs_entity,
			      server);
	p = cbs_task_of(cbs_se);
	rq = task_rq_lock(p, &rflags); /* Returns the rq this task belongs to */
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

	if (cbs_se->on_rq) {
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
		} else {
			struct rb_node *leftmost;
			/* if task on CPU isn't the leftmost on rq, resched it */
			leftmost = rb_first_cached(&cbs_rq->tasks_tree);
			if(leftmost) {
				struct sched_cbs_entity *leftmost_se;
				leftmost_se = container_of(leftmost,
							   struct sched_cbs_entity,
							   rb_node);
				if(cbs_task_of(leftmost_se) != rq->curr)
					resched_curr(rq);
			}
		}
	}

	trace_printk("[id:%d] Budget Replenished [Budget=%llu] [D=%llu]\n",
		     cbs_se->id,
		     (unsigned long long)cbs_se->server.remaining_budget,
		     (unsigned long long)cbs_se->deadline);

	raw_spin_unlock(&cbs_rq->lock);

	task_rq_unlock(rq, p, &rflags);

	return HRTIMER_NORESTART;
}


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


static int sched_cbs_entity_hr_replenish_disarm(struct sched_cbs_entity *p)
{
	struct hrtimer *timer;
	int ret;

	timer = &p->server.hr_replenish;

	ret = hrtimer_try_to_cancel(timer);

	return ret;
}


static void sched_cbs_entity_hr_timers_setup(struct sched_cbs_entity *p)
{
	sched_cbs_entity_hr_deadline_setup(p);

	if(sched_cbs_entity_is_hard(p))
		return;

	sched_cbs_entity_hr_replenish_setup(p);
}


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

	// 4. arm deadline timer
	sched_cbs_entity_hr_deadline_arm(cbs_se);

	// 5. mark task as part of the rq
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


static bool dequeue_task_cbs(struct rq *rq, struct task_struct *p, int flags)
{
        struct sched_cbs_entity *cbs_se;
        struct cbs_rq           *cbs_rq;
	int is_disarmed;
	u64 now;

	raw_spin_lock(&rq->cbs.lock);

	cbs_se = &p->cbs;
	cbs_rq = &rq->cbs;

	if(!cbs_se->on_rq) {
		raw_spin_unlock(&rq->cbs.lock);
		return false;
	}

	// 1. disarm hr_deadline
	is_disarmed = sched_cbs_entity_hr_deadline_disarm(cbs_se);
	trace_printk("[id:%d] Dead disarmed [status:%d]\n",
		     cbs_se->id, is_disarmed);

	// 2. erase task from rq
	rb_erase_cached(&cbs_se->rb_node, &cbs_rq->tasks_tree);
	RB_CLEAR_NODE(&cbs_se->rb_node);

	now = ktime_get_ns();

	// 3. update remaining_budget
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


/* @p: task that wants CPU */
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


/* @p: task to run next on CPU */
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


static int select_task_rq_cbs(struct task_struct *p, int cpu, int flags)
{
        return cpu;
}


static void task_tick_cbs(struct rq *rq, struct task_struct *p, int queued)
{
	update_curr_cbs(rq);
}


static void prio_changed_cbs(struct rq *rq, struct task_struct *p, u64 oldprio)
{}


static void switched_to_cbs(struct rq *rq, struct task_struct *task)
{}


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
