#ifndef __SCHED_CBS_SYSCALLS_H
#define __SCHED_CBS_SYSCALLS_H

#include <linux/types.h>

int do_moker_sched_cbs_entity_setup(int id, u64 runtime, u64 period, u64 deadline, int is_hard);

#endif /* __SCHED_CBS_SYSCALLS_H */
