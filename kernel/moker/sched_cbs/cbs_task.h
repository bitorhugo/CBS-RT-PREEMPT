/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __CBS_TASK_H_
#define __CBS_TASK_H_

#include <linux/rbtree_types.h>
#include <linux/types.h>
#include <linux/hrtimer_types.h>

struct sched_cbs_entity_server {
	u64 capacity;
	u64 remaining_budget; /* relative timestamp in ns */

	struct hrtimer hr_replenish;

	int first; /* -1 if not */
}

struct sched_cbs_entity {
        struct rb_node rb_node;

        u64 runtime; /* hard:Ci, soft:cs */
        u64 period;
	u64 deadline; /* absolute timestamp in ns */
	u64 bandwidth; /* runtime|period */
	u64 exec_start; /* marks the instance task started running in CPU */

	struct hrtimer hr_deadline;

	struct sched_cbs_entity_server server;

	int on_rq;
	int id;
};


#endif /* __CBS_TASK_H_ */
