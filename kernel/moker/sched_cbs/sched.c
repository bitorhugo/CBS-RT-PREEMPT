// SPDX-License-Identifier: GPL-2.0

#include "cbs_rq.h"
#include "cbs_task.h"
#include "../../sched/sched.h"
#ifdef CONFIG_MOKER_TRACING
#include "../trace/trace.h"
#endif


static inline struct task_struct *cbs_task_of(struct sched_cbs_entity *cbs_se)
{
	return container_of(cbs_se, struct task_struct, cbs);
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


static void sched_cbs_entity_hr_deadline_disarm(struct sched_cbs_entity *p)
{
	struct hrtimer *timer;

	timer = &p->hr_deadline;

	hrtimer_try_to_cancel(timer);
}


static void enqueue_task_cbs(struct rq *rq, struct task_struct *p, int flags)
{
        struct sched_cbs_entity *cbs_se;
        struct cbs_rq *cbs_rq;
        u64 now;

	raw_spin_lock(&rq->cbs.lock);

	cbs_se = &p->cbs;
	cbs_rq = &rq->cbs;

	if(cbs_se->on_rq) {
		raw_spin_unlock(&rq->cbs.lock);
		return;
	}

	now = rq_clock_task(rq);

	// 1. calculate deadline, d = now + period
	cbs_se->deadline = now + cbs_se->period;
	trace_printk("MOKER: [id:%d] Calculated Deadline[:%llu]\n",
		     cbs_se->id, (unsigned long long)cbs_se->deadline);

	// 2. insert in tree
	rb_add_cached(&cbs_se->rb_node, &cbs_rq->tasks_tree, cbs_rq_less);
	trace_printk("MOKER: [id:%d] Inserted on tree\n", cbs_se->id);

	// 3. setup deadline timer
	sched_cbs_entity_hr_deadline_setup(cbs_se);

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

	raw_spin_lock(&rq->cbs.lock);

	cbs_se = &p->cbs;
	cbs_rq = &rq->cbs;

	if(!cbs_se->on_rq) {
		raw_spin_unlock(&rq->cbs.lock);
		return false;
	}

	// 1. disarm hr_deadline
	sched_cbs_entity_hr_deadline_disarm(cbs_se);

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

        if (curr->sched_class != &cbs_sched_class) {
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
{}


static void set_next_task_cbs(struct rq *rq, struct task_struct *p, bool first)
{
	struct sched_cbs_entity *cbs_se;

	cbs_se = &p->cbs;

	if(!first)
		return;

	/* Start deadline countdown */
	sched_cbs_entity_hr_deadline_arm(cbs_se);
}


static int select_task_rq_cbs(struct task_struct *p, int cpu, int flags)
{
        return cpu;
}


static void task_tick_cbs(struct rq *rq, struct task_struct *p, int queued)
{}


static void prio_changed_cbs(struct rq *rq, struct task_struct *p, u64 oldprio)
{}


static void switched_to_cbs(struct rq *rq, struct task_struct *task)
{}


static void update_curr_cbs(struct rq *rq)
{}


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
