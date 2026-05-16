#include "defs.h"
#include <stdio.h>
#define _GNU_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <linux/unistd.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <sys/syscall.h>
#include <pthread.h>

#ifdef __x86_64__
#define __NR_sched_setattr           314
#define __NR_sched_getattr           315
#endif

struct sched_attr {
	__u32 size;

	__u32 sched_policy;
	__u64 sched_flags;

	/* SCHED_NORMAL, SCHED_BATCH */
	__s32 sched_nice;

	/* SCHED_FIFO, SCHED_RR */
	__u32 sched_priority;

	/* SCHED_DEADLINE (nsec) */
	__u64 sched_runtime;
	__u64 sched_deadline;
	__u64 sched_period;
};


int sched_setattr(pid_t pid,
		  const struct sched_attr *attr,
		  unsigned int flags)
{
	return syscall(__NR_sched_setattr, pid, attr, flags);
}


int sched_getattr(pid_t pid,
		  struct sched_attr *attr,
		  unsigned int size,
		  unsigned int flags)
{
	return syscall(__NR_sched_getattr, pid, attr, size, flags);
}

void do_work(unsigned long long exec)
{
	unsigned long long ten_ns;

	ten_ns = exec / 10;

	for(size_t i = 0; i < ten_ns; i++) {
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);

		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);

		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);
		asm volatile  ("nop" ::);

	}
}

int main(int argc, char **argv)
{
	unsigned long long C;
	unsigned long long T;
	unsigned long long O;
	unsigned long long time0;
	unsigned long long release;
	unsigned int task_id;
	unsigned int njobs;
	int res;
	/* struct sched_param param; */
	struct sched_attr attr;
	struct timespec r;

	task_id = atoi(argv[1]);
	printf("Task(%d, %d): before SCHED_CBS\n", task_id, getpid());

	C = (unsigned long long)atoll(argv[2]);
	T = (unsigned long long)atoll(argv[3]);
	O = (unsigned long long)atoll(argv[4]) + OFFSET;
	time0 = (unsigned long long)atoll(argv[5]);
	njobs = atoi(argv[6]);

	/* param.sched_priority = 0; */

	attr.size = sizeof(attr);
	attr.sched_flags = 0;
	attr.sched_nice = 0;
	attr.sched_priority = 0;
	attr.sched_policy = SCHED_CBS;
	attr.sched_runtime = C;
	attr.sched_period = T;
	attr.sched_deadline = T;

	/*
	  res = sched_setscheduler(0, SCHED_CBS, &param);
	  if(res == -1)
	  goto err_sched_setscheduler;
	*/

	ret = sched_setattr(0, &attr, flags);
	if(ret < 0) {
		goto err_sched_setattr;
	}

	printf("Task(%d, %d): after SCHED_CBS\n", task_id, getpid());

	printf("Task(%d,%d): set ID\n", task_id,getpid());
	if((syscall(SYS_MOKER_ID, task_id)) < 0) {
		perror("ERROR: Setting moker id failed");
		exit(-1);
	}


	release = time0 + O;

	for(int i = 0; i < njobs; i++) {
		r.tv_sec = release / NSEC_PER_SEC;
		r.tv_nsec = release % NSEC_PER_SEC;

		printf("Task(%d, %d, %d): sleeping until %lld\n",
		       task_id, getpid(), i, release);

		clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &r, NULL);
		printf("Task(%d, %d, %d): ready for execution\n",
		       task_id, getpid(), i);

		do_work(C);

		release += T; /* computes the next release */
	}

	exit(task_id);

err_sched_setattr:
	perror("ERROR:sched_setattr failed");
	exit(res);

err_sched_setscheduler:
	perror("ERROR:sched_setscheduler failed");
	exit(res);
}
