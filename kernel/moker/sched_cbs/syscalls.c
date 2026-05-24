#include <linux/syscalls.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/math64.h>
#include "../../sched/sched.h"
#include "syscalls.h"
#include "cbs_task.h"

#ifdef CONFIG_MOKER_TRACING
#include "../trace/trace.h"
#endif

/**
 * moker_sched_cbs_entity_setup - syscall to setup a CBS entity for current task
 * @id: identifier for the CBS entity
 * @runtime: execution budget (Ci) in nanoseconds
 * @period: server period (Ti) in nanoseconds
 * @deadline: initial absolute deadline (Di) in nanoseconds
 * @is_hard: non-zero if entity is hard real-time (no budget tracking)
 *
 * Logs the request and forwards parameters to do_moker_sched_cbs_entity_setup.
 * Return: syscall return value from do_moker_sched_cbs_entity_setup.
 */
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

/**
 * do_moker_sched_cbs_entity_setup - configure current task CBS parameters
 * @id: entity identifier
 * @runtime: execution budget (Ci)
 * @period: server period (Ti)
 * @deadline: initial deadline (Di)
 * @is_hard: non-zero if hard real-time server
 *
 * Initialize the current task's `cbs` fields, compute soft-server capacity,
 * and switch the task's scheduler to SCHED_CBS.
 * Return: result of `sched_setscheduler` call.
 */
int do_moker_sched_cbs_entity_setup(int id, u64 runtime, u64 period,
				    u64 deadline, int is_hard)
{
	u64 cap;

	current->cbs.id       = id;
	current->cbs.runtime  = runtime;
	current->cbs.period   = period;
	current->cbs.deadline = deadline;

	if(!is_hard) {
		cap = mul_u64_u64_div_u64(current->cbs.runtime, 80, 100);
		current->cbs.server = (struct sched_cbs_entity_server){ .capacity = cap,
			.first = 1};
	} else {
		current->cbs.server = (struct sched_cbs_entity_server){ 0 };
	}

	return sched_setscheduler(current,
				  SCHED_CBS,
				  &(struct sched_param){ .sched_priority = 0 });
}
