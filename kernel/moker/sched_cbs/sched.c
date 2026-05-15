// SPDX-License-Identifier: GPL-2.0

#include "../../sched/sched.h"
#include "cbs_rq.h"
#include "cbs_task.h"
#ifdef CONFIG_MOKER_TRACING
#include "../TRACE/trace.h"
#endif


static void enqueue_task_cbs(struct rq *rq, struct task_struct *p, int flags)
{
        struct sched_cbs_entity *cbs_se;
        struct cbs_rq *cbs_rq;
        u64 now;

	cbs_se = &p->cbs;
	cbs_rq = &rq->cbs;

	if(WARN_ON_ONCE(cbs_se->on_rq))
		return;

	now = rq_clock_task(rq);

	// 1. calculate deadline, d = now + period
	cbs_se->deadline = now + cbs_se->period;
	pr_info("Moker[id:%d]: Calculated Deadline[:%llu]\n",
		cbs_se->id, (unsigned long long)cbs_se->deadline);

	// 2. insert on tree
	rb_add_cached(&cbs_se->rb_node, &cbs_rq->tasks_tree, cbs_rq_less);

	// 3. mark task as part of the rq
        cbs_se->on_rq = 1;
        add_nr_running(rq, 1);

#ifdef CONFIG_MOKER_TRACING
        moker_trace(ENQUEUE_RQ, p, cbs_se->id);
#endif
}


static bool dequeue_task_cbs(struct rq *rq, struct task_struct *p, int flags)
{
        struct sched_cbs_entity *cbs_se;
        struct cbs_rq           *cbs_rq;

	cbs_se = &p->cbs;
	cbs_rq = &rq->cbs;

	if(WARN_ON_ONCE(!cbs_se->on_rq))
		return false;

	// 1. erase task from rq
	rb_erase_cached(&cbs_se->rb_node, &cbs_rq->tasks_tree);

	// 2. mark task as not part of the rq
	cbs_se->on_rq = 0;
	sub_nr_running(rq, 1);

#ifdef CONFIG_MOKER_TRACING
        moker_trace(DEQUEUE_RQ, p, cbs_se->id);
#endif
        return true;
}


static void wakeup_preempt_cbs(struct rq *rq, struct task_struct *p, int flags)
{
        struct task_struct *curr;
	s64 diff;

	curr = rq->curr;

        if (curr->sched_class != &cbs_sched_class) {
                resched_curr(rq);
		return;
	}

	diff = (s64)(p->cbs.deadline - curr->cbs.deadline);

        if (diff < 0) {
		pr_info("Moker: Preempting: in:[id:%d] out:[id:%d]\n",
			curr->cbs.id, p->cbs.id);
                resched_curr(rq);
	}
}


static struct task_struct *pick_task_cbs(struct rq *rq, struct rq_flags *rf)
{
	struct task_struct *picked;
	struct rb_node *leftmost;

        if (rq->cbs.nr_running < 1)
                return NULL;

	leftmost = rb_first_cached(&rq->cbs.tasks_tree);
	if (!leftmost)
		return NULL;

	picked = container_of(leftmost, struct task_struct, cbs.rb_node);
	pr_info("Moker: Picked [id:%d]\n", picked->cbs.id);

        return picked;
}


static void put_prev_task_cbs(struct rq *rq, struct task_struct *p,
                               struct task_struct *next)
{}


static void set_next_task_cbs(struct rq *rq, struct task_struct *task, bool first)
{}


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
{
	pr_info("Moker: Updating..\n");
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
