#include "cbs_rq.h"
#include "cbs_task.h"
#include <linux/rbtree.h>

/**
 * init_cbs_rq - initializes the SCHED_CBS runqueue
 * @rq: the CBS runqueue to initialize
 */
void init_cbs_rq(struct cbs_rq *rq)
{
        rq->tasks_tree = RB_ROOT_CACHED;
	raw_spin_lock_init(&rq->lock);
}

/**
 * cbs_rq_less - operator defining the (partial) node order
 * @this: node to insert
 * @that: node to compare against
 *
 * Compares two CBS entities by deadline to establish ordering
 * in the red-black tree. Uses a signed 64-bit subtraction to
 * handle wrap-around correctly, following the same pattern
 * used by other kernel scheduling classes.
 *
 * Return: true if @this has an earlier deadline than @that.
 */
bool cbs_rq_less(struct rb_node *this, const struct rb_node *that)
{
	struct sched_cbs_entity *se_this;
	struct sched_cbs_entity *se_that;
	s64 diff;

	se_this = rb_entry(this, struct sched_cbs_entity, rb_node);
	se_that = rb_entry(that, struct sched_cbs_entity, rb_node);

	/* Most scheduling classes do comparisons by:
	 * 1. coercing to 64bit the value of some arithmetic op
	 * 2. comparing to 0
	 */
	diff = (s64)(se_this->deadline - se_that->deadline);

	return diff < 0;
}
