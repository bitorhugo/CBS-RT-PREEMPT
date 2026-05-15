/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __CBS_TASK_H_
#define __CBS_TASK_H_

#include <linux/rbtree_types.h>
#include <linux/types.h>

struct sched_cbs_entity {
        struct rb_node rb_node;

        u64 runtime;
        u64 period;
	u64 deadline;

	int on_rq;
	int id;
};


#endif /* __CBS_TASK_H_ */
