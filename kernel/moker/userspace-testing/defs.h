
#ifndef DEFS_H_
#define DEFS_H_

#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/syscall.h>



#define NR_TASKS		50
#define BUF_SIZE		10000

#define OFFSET			1000000000L

#define NSEC_PER_SEC	1000000000L
#define SCHED_CBS		8

#define SYS_MOKER_TRACING_ENABLE 471
#define SYS_MOKER_ID 472


struct task {
	int id;
	unsigned long long C; //exec
	unsigned long long T; //period
	unsigned long long O; //first job offset
	unsigned int njobs; //number of jobs
};

#endif
