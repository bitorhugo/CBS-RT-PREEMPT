#include "defs.h"
#include <stdio.h>

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

int main(void)
{
	unsigned long long C;
	unsigned long long T;
	unsigned long long O;
	unsigned long long time0;
	unsigned long long release;
	unsigned int task_id;
	unsigned int njobs;
	int res;
	struct sched_param param;
	struct timespec r;

	task_id = atoi(argv[1]);
	printf("Task(%d, %d): before SCHED_CBS\n", task_id, getpid());

	C = (unsigned long long)atoll(argv[2]);
	T = (unsigned long long)atoll(argv[3]);
	O = (unsigned long long)atoll(argv[4]) + OFFSET;
	time0 = (unsigned long long)atoll(argv[5]);
	njobs = atoi(argv[6]);

	param.sched_priority = 0;
	res = sched_setscheduler(0, SCHED_CBS, &param);
	if(res == -1)
		goto err_sched_setscheduler;

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

err_sched_setscheduler:
	perror("ERROR:sched_setscheduler failed");
	exit(res);
}
