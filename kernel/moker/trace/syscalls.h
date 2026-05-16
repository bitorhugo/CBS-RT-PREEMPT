#ifndef __SYSCALLS_H
#define __SYSCALLS_H

int set_moker_tracing (unsigned int t);
int do_moker_sched_cbs_entity_setup (int id, u64 runtime, u64 period, u64 deadline);

#endif
