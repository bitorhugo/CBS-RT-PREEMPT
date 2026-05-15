#ifndef __CBS_RQ_H_
#define __CBS_RQ_H_

#include <linux/sched.h>
#include <linux/rbtree_types.h>
#include <linux/spinlock.h>
#include "cbs_task.h"

struct cbs_rq {
        struct rb_root_cached   tasks_tree; /* leftmost node is memoized */
	raw_spinlock_t          lock;
	unsigned int            nr_running;
};


void init_cbs_rq(struct cbs_rq *rq);
bool cbs_rq_less(struct rb_node *this, const struct rb_node *parent);


#endif /* __CBS_RQ_H_ */
