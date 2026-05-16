// SPDX-License-Identifier: GPL-2.0

#include "cbs_rq.h"
#include "cbs_task.h"
#include "../../sched/sched.h"
#ifdef CONFIG_MOKER_TRACING
#include "../trace/trace.h"
#endif


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
	return !p->server;
}


static enum hrtimer_restart sched_cbs_entity_hr_deadline_callback(struct hrtimer *timer)
{
	struct rq *rq;
	struct rq_flags rflags;
	struct task_struct *p;
	struct sched_cbs_entity *cbs_se;

	cbs_se = container_of(timer, struct sched_cbs_entity, hr_deadline);
	p = cbs_task_of(cbs_se);

	rq = task_rq_lock(p, &rflags); /* Grab the rq this task belongs to */

	update_rq_clock(rq); /* Requires rq_lock */

	/*
	 * We don't actually want to do anything but log something.
	 * We assume schedulability analysis was performed.
	 */
	trace_printk("MOKER: [id:%d] Deadline not met\n", cbs_se->id);

	task_rq_unlock(rq, p, &rflags);

	return HRTIMER_NORESTART;
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
	struct task_struct *p;
	struct sched_cbs_entity *cbs_se;

	server = container_of(timer, struct sched_cbs_entity_server, hr_replenish);
	/* TODO: Impl cbs_se_of(server) */
	cbs_se = container_of(server, struct sched_cbs_entity, server);
	p = cbs_task_of(cbs_se);

	rq = task_rq_lock(p, &rflags); /* Grab the rq this task belongs to */

	update_rq_clock(rq); /* Requires rq_lock */

	/* TODO */

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
	mode = HRTIMER_MODE_ABS_HARD;

	hrtimer_setup(timer,
		      sched_cbs_entity_hr_replenish_callback,
		      clock_id,
		      mode);
}


static void sched_cbs_entity_hr_replenish_arm(struct sched_cbs_entity *p)
{
	struct hrtimer *timer;
	ktime_t remaining_budget;
	enum hrtimer_mode mode;

	timer = &p->server.hr_replenish;
	remaining_budget = ns_to_ktime(p->runtime);
	mode = HRTIMER_MODE_ABS_HARD;

	hrtimer_start(timer, remaining_budget, mode);
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
	s64 diff;
	u64 lhs;
	u64 rhs;

	if(sched_cbs_entity_is_hard(p)) {
		p->deadline = arrival + p->period; /* Di = Ti */
		return;
	}

	/* cs * Ts >= (di - ri) * Qs/Ts */

	diff = (s64)(p->deadline - arrival);
	if (diff <= 0) {
		p->deadline = arrival + p->period;
		return;
	}

	lhs = p->runtime * p->period;
	rhs = (u64)diff * p->server.capacity;
	if (lhs >= rhs)
		p->deadline = arrival + p->period;
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
	trace_printk("MOKER: [id:%d] [deadline:%llu]\n",
		     cbs_se->id, (unsigned long long)cbs_se->deadline);

	// 2. insert in tree
	rb_add_cached(&cbs_se->rb_node, &cbs_rq->tasks_tree, cbs_rq_less);
	trace_printk("MOKER: [id:%d] Inserted on tree\n", cbs_se->id);

	// 3. setup deadline & replenish timers
	sched_cbs_entity_hr_timers_setup(cbs_se);

	// 4. arm deadline timer
	sched_cbs_entity_hr_deadline_arm(cbs_se);
	trace_printk("MOKER: [id:%d] Dead armed\n", cbs_se->id);

	// 4. mark task as part of the rq
        cbs_se->on_rq = 1;
	cbs_rq->nr_running++;
        add_nr_running(rq, 1);

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

	raw_spin_lock(&rq->cbs.lock);

	cbs_se = &p->cbs;
	cbs_rq = &rq->cbs;

	if(!cbs_se->on_rq) {
		raw_spin_unlock(&rq->cbs.lock);
		return false;
	}

	// 1. disarm hr_deadline
	is_disarmed = sched_cbs_entity_hr_deadline_disarm(cbs_se);
	trace_printk("MOKER: [id:%d] Dead disarmed [status:%d]\n",
		     cbs_se->id, is_disarmed);

	// 2. erase task from rq
	rb_erase_cached(&cbs_se->rb_node, &cbs_rq->tasks_tree);
	trace_printk("MOKER: [id:%d] Erased from tree\n", cbs_se->id);

	// 3. mark task as not part of the rq
	cbs_se->on_rq = 0;
	cbs_rq->nr_running--;
	sub_nr_running(rq, 1);

	raw_spin_unlock(&rq->cbs.lock);

#ifdef CONFIG_MOKER_TRACING
        moker_trace(DEQUEUE_RQ, p, cbs_se->id);
#endif
        return true;
}


static void wakeup_preempt_cbs(struct rq *rq, struct task_struct *p, int flags)
{
        struct task_struct *curr;

	curr = rq->curr;

        if (!task_has_cbs_policy(p)) {
                resched_curr(rq);
		return;
	}

        if (p->cbs.deadline < curr->cbs.deadline) {
		trace_printk("MOKER: [id:%d] Preempted by [id:%d]\n",
			     curr->cbs.id, p->cbs.id);
                resched_curr(rq);
	}
}


static struct task_struct *pick_task_cbs(struct rq *rq, struct rq_flags *rf)
{
	struct task_struct *picked;
	struct rb_node *leftmost;

	raw_spin_lock(&rq->cbs.lock);

	/* TODO: Remove. Not needed. We use leftmost memoized tree. */
        if (rq->cbs.nr_running < 1) {
		raw_spin_unlock(&rq->cbs.lock);
                return NULL;
	}

	leftmost = rb_first_cached(&rq->cbs.tasks_tree);
	if (!leftmost) {
		raw_spin_unlock(&rq->cbs.lock);
		return NULL;
	}

	picked = cbs_task_of(container_of(leftmost,
					  struct sched_cbs_entity,
					  rb_node));
	trace_printk("MOKER: [id:%d] Picked\n", picked->cbs.id);

	raw_spin_unlock(&rq->cbs.lock);

	return picked;
}


static void put_prev_task_cbs(struct rq *rq, struct task_struct *p,
			      struct task_struct *next)
{
	struct sched_cbs_entity *cbs_se;
	int is_disarmed;

	if(sched_cbs_entity_is_hard(cbs_se))
		return;

	// 1. update remaining_runtime
	update_curr_cbs(rq);

	// 2. cancel the budget tracking timer
	is_disarmed = sched_cbs_entity_hr_replenish_disarm(cbs_se);
	trace_printk("MOKER: [id:%d] Replen disarmed [status:%d]\n",
		     cbs_se->id, is_disarmed);
}


static void set_next_task_cbs(struct rq *rq, struct task_struct *p, bool first)
{
	struct sched_cbs_entity *cbs_se;
	u64 now;

	cbs_se = &p->cbs;
	now = ktime_get_ns();

	if(!first)
		return;

	if(sched_cbs_entity_is_hard(cbs_se))
		return;

	// 1. set remaining_runtime
	cbs_se->remaining_runtime = now;

	// 2. arm replenish
	sched_cbs_entity_hr_replenish_arm(cbs_se);
	trace_printk("MOKER: [id:%d] Replen armed\n", cbs_se->id);
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
	struct task_struct *donor; /* Add documentation for donor */
	struct sched_cbs_se *cbs_se;
	u64 now;
	u64 delta;

	donor = rq->donor;
	cbs_se = &donor;
	now = ktime_get_ns();

	if(sched_cbs_entity_is_hard(cbs_se))
		return;

	delta = now - cbs_se->remaining_runtime;
	cbs_se->remaining_runtime = delta;
	trace_printk("MOKER: [id:%d] [rem_runtime:%llu]\n",
		     cbs_se->id, delta);
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
