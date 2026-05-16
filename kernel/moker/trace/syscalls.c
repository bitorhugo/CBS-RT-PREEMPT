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


int do_moker_sched_cbs_entity_setup(int id, u64 runtime, u64 period, u64 deadline)
{
	current->cbs.id       = id;
	current->cbs.runtime  = runtime;
	current->cbs.period   = period;
	current->cbs.deadline = deadline;

	return sched_setscheduler(current,
				  SCHED_CBS,
				  &(struct sched_param){ .sched_priority = 0});

}


SYSCALL_DEFINE4(do_moker_sched_cbs_entity_setup,
		int, id,
		u64, runtime,
		u64, period,
		u64, deadline)
{
	pr_info("MOKER: sys_moker_id_c_t_d:[pid=%d][id=%d][c=%llu][t=%llu][d=%llu]\n",
		current->pid, id, runtime, period, deadline);

	#ifdef CONFIG_MOKER_SCHED_CBS_POLICY
	return do_moker_sched_cbs_entity_setup(id, runtime, period, deadline);
	#else
	return -1;
	#endif
}
