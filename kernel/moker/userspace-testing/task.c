#include "defs.h"
#include <stdio.h>


void do_work(unsigned long long exec)
{
	unsigned long long ten_ns;

	ten_ns = (exec * 2) / 17;

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
	unsigned long long D;
	unsigned long long O;
	unsigned long long time0;
	unsigned long long release;
	unsigned int task_id;
	unsigned int is_hard;
	unsigned int njobs;
	struct timespec r;

	task_id = atoi(argv[1]);
	is_hard = atoi(argv[2]);
	printf("Task(id[%d], pid[%d], is_hard[%d]): before SCHED_CBS\n",
	       task_id, getpid(), is_hard);

	C = (unsigned long long)atoll(argv[3]);
	T = D = (unsigned long long)atoll(argv[4]);
	O = (unsigned long long)atoll(argv[5]) + OFFSET;
	time0 = (unsigned long long)atoll(argv[6]);
	njobs = atoi(argv[7]);

	printf("Task(id[%d], pid[%d], is_hard[%d]): setup ID\n",
	       task_id, getpid(), is_hard);

	if((syscall(SYS_MOKER_TASK_SETUP, task_id, C, T, D, is_hard)) < 0) {
		perror("ERROR: Setting MOKER_TASK_SETUP failed");
		exit(-1);
	}

	printf("Task(id[%d], pid[%d], is_hard[%d]): after SCHED_CBS\n",
	       task_id, getpid(), is_hard);

	release = time0 + O;

	for(int i = 0; i < njobs; i++) {
		r.tv_sec = release / NSEC_PER_SEC;
		r.tv_nsec = release % NSEC_PER_SEC;

		printf("Task(id[%d], pid[%d], is_hard[%d], job[%d]): sleeping until %lld\n",
		       task_id, getpid(), is_hard, i, release);

		clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &r, NULL);
		printf("Task(id[%d], pid[%d], is_hard[%d], job[%d]): ready for execution\n",
		       task_id, getpid(), is_hard, i);

		do_work(C);

		release += T; /* computes the next release */
	}

	exit(task_id);
}
