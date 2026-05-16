#include <linux/syscalls.h>
#include "../../sched/sched.h"
#include "trace.h"

SYSCALL_DEFINE1(moker_tracing, unsigned int, toggle)
{
	if (toggle != 0 && toggle != 1)
		goto err;

	pr_info("MOKER: moker_tracing:[%d][%d]\n", (int)toggle, current->pid);

	return set_moker_tracing(toggle);

err:
	pr_err("MOKER: invalid argument value.. Must be either 1(ON) or 0(OFF)");
	return -EINVAL;
}


int set_moker_tracing(unsigned int toggle)
{
#ifdef CONFIG_MOKER_TRACING
	pr_info("MOKER: sys_moker_tracing:[%d][%d]\n", (int)toggle, current->pid);
	set_tracing(toggle);
#endif
	return 0;
}



SYSCALL_DEFINE5(moker_sched_cbs_entity_setup,
		int, id,
		u64, runtime,
		u64, period,
		u64, deadline,
		int, is_hard)
{
	pr_info("MOKER: sys_sched_cbs_entity_setup:[pid=%d][id=%d][is_hard=%d][c=%llu][t=%llu][d=%llu]\n",
		current->pid, id, is_hard, runtime, period, deadline);

	#ifdef CONFIG_MOKER_SCHED_CBS_POLICY
	return do_moker_sched_cbs_entity_setup(id, runtime, period, deadline, is_hard);
	#else
	return -1;
	#endif
}

int do_moker_sched_cbs_entity_setup(int id, u64 runtime, u64 period, u64 deadline, int is_hard)
{
	current->cbs.id       = id;
	current->cbs.runtime  = runtime;
	current->cbs.period   = period;
	current->cbs.deadline = deadline;
	if(is_hard)
		currnet->cbs_server   = NULL;

	return sched_setscheduler(current,
				  SCHED_CBS,
				  &(struct sched_param){ .sched_priority = 0});

}
