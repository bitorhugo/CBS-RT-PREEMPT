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
	struct timespec r;

	task_id = atoi(argv[1]);
	printf("Task(%d, %d): before SCHED_CBS\n", task_id, getpid());

	C = (unsigned long long)atoll(argv[2]);
	T = (unsigned long long)atoll(argv[3]);
	O = (unsigned long long)atoll(argv[4]) + OFFSET;
	time0 = (unsigned long long)atoll(argv[5]);
	njobs = atoi(argv[6]);

	printf("Task(%d,%d): setup ID\n", task_id, getpid());
	if((syscall(SYS_MOKER_TASK_SETUP, task_id, C, T, D)) < 0) {
		perror("ERROR: Setting moker task setup failed");
		exit(-1);
	}

	printf("Task(%d, %d): after SCHED_CBS\n", task_id, getpid());

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
