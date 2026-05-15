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


SYSCALL_DEFINE1(moker_id, int, id)
{
	pr_info("MOKER: sys_moker_id:[%d][%d]\n", id, current->pid);

	return do_moker_id(id);
}

int do_moker_id (int id)
{
#ifdef CONFIG_MOKER_SCHED_CBS_POLICY
	pr_info("MOKER: set_moker_id:[%d][%d]\n", id, current->pid);

	current->cbs.id = -1;
	if(cbs_policy(current->policy)) {
		current->cbs.id = id;
	}
#endif
	return 0;
}
