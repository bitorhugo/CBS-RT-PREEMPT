#include <linux/syscalls.h>
#include "../../sched/sched.h"

SYSCALL_DEFINE1(moker_id, int, id)
{
	pr_info("MOKER: sys_moker_id:[%d][%d]\n", id, current->pid);

	return set_moker_id(id);
}

int set_moker_id (int id)
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
