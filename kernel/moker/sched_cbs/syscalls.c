#include <linux/syscalls.h>
#include <linux/sched.h>
#include <linux/string.h>
#include "../../sched/sched.h"
#include "syscalls.h"
#include "cbs_task.h"

#ifdef CONFIG_MOKER_TRACING
#include "../trace/trace.h"
#endif

SYSCALL_DEFINE5(moker_sched_cbs_entity_setup,
		int, id,
		u64, runtime,
		u64, period,
		u64, deadline,
		int, is_hard)
{
	pr_info("MOKER: sys_sched_cbs_entity_setup:[pid=%d][id=%d][is_hard=%d][c=%llu][t=%llu][d=%llu]\n",
		current->pid,
		id,
		is_hard,
		(unsigned long long)runtime,
		(unsigned long long)period,
		(unsigned long long)deadline);

	return do_moker_sched_cbs_entity_setup(id, runtime, period, deadline, is_hard);
}

int do_moker_sched_cbs_entity_setup(int id, u64 runtime, u64 period, u64 deadline, int is_hard)
{
	current->cbs.id       = id;
	current->cbs.runtime  = runtime;
	current->cbs.period   = period;
	current->cbs.deadline = deadline;

	if (is_hard) {
		current->cbs.server = (struct sched_cbs_entity_server){ .capacity = runtime };
	} else {
		memset(&current->cbs.server, 0, sizeof(struct sched_cbs_entity_server));
	}

	return sched_setscheduler(current,
				  SCHED_CBS,
				  &(struct sched_param){ .sched_priority = 0 });
}
