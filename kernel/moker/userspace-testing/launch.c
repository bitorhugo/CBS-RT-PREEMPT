#include "defs.h"


void print_task(struct task *task)
{
	printf("TASK:%d\t%d\t%llu\t%llu\t%llu\t%u\n",
	       task->id,
	       task->is_hard,
	       task->C,
	       task->T,
	       task->O,
	       task->njobs);
}

void print_tasks(struct task *tasks, int num)
{
	int i;

	for(i = 0; i < num; i++) {
		print_task(&tasks[i]);
	}
}

unsigned int get_u32(char *s)
{
	int i;
	int d;
	unsigned int v;

	d = 0;
	v = 0;

	for(i = 0; s[i] >= '0' && s[i] <= '9'; i++) {
		d = s[i] - '0';
		v = (v * 10) + d;
	}

	return v;
}

unsigned long long get_u64(char *s)
{
	int i;
	int d;
	unsigned long long v;

	d = 0;
	v = 0;

	for(i = 0; s[i] >= '0' && s[i] <= '9'; i++) {
		d = s[i] - '0';
		v = (v * 10) + d;
	}

	return v;
}

void get_task_info(char * str, struct task *task)
{
	char *s;
	char *s1;
	int i;

	i = 0;
	s = s1 = str;

	while(i < 6) {
		if(*s == ',') {
			*s = '\0';

			switch(i) {
			case 0:
				task->id = get_u32(s1);
				s1 = s + 1;
				i++;
				break;
			case 1:
				task->is_hard = get_u32(s1);
				s1 = s + 1;
				i++;
				break;
			case 2:
				task->C = get_u64(s1);
				s1 = s + 1;
				i++;
				break;
			case 3:
				task->T = get_u64(s1);
				s1 = s + 1;
				i++;
				break;
			case 4:
				task->O = get_u64(s1);
				s1 = s + 1;
				i++;
				break;
			case 5:
				task->njobs = get_u32(s1);
				s1 = s + 1;
				i++;
				break;
			}
		}
		s++;
	}
}



void get_taskset_config(char *file, unsigned int *ntasks, struct task *tasks)
{
	char buffer[BUF_SIZE];
	unsigned int i = 0;
	FILE* fd;

	fd = fopen(file, "r");

	buffer[0] = 0;
	(*ntasks) = 0;

	while((fgets(buffer, BUF_SIZE, fd)) != NULL) {
		if(buffer[0] >= '0' || buffer[0] <= '9') {
			get_task_info(buffer, &tasks[i]);
			i++;
		}
		buffer[0] = 0;
	}

	if(i <= 0) {
		printf("Error: ntasks parameter(%d) invalid\n",i);
		exit(0);
	}

	(*ntasks) = i;

	fclose(fd);
}

int main(int argc, char *argv[])
{
	pid_t pid_tasks[NR_TASKS];
	struct task tasks[NR_TASKS];
	unsigned int ntasks;
	char arg[7][30];
	struct timespec t;
	unsigned long long time0;
	int status;
	int i;

	if(argc != 2)
		exit(0);

	get_taskset_config(argv[1], &ntasks, tasks);

	print_tasks(tasks, ntasks);

	printf("LAUNCH: Enabling MOKER_TRACING\n");
	if((syscall(SYS_MOKER_TRACING_ENABLE, 1)) < 0) {
		perror("ERROR: Enabling MOKER_TRACING failed");
		exit(-1);
	}

	clock_gettime(CLOCK_MONOTONIC, &t);
	time0 = t.tv_sec * NSEC_PER_SEC;
	time0 += t.tv_nsec;

	time0 += (unsigned long long)(5 * OFFSET); //for safety purposes

	printf("LAUNCH: time 0: %llu\n", time0);

	printf("LAUNCH: Forking tasks\n");
	for(i = 0; i < ntasks; i++) {
		char task_name[16];
		sprintf(task_name, "task-%d", tasks[i].id);
		sprintf(arg[0],"%d", tasks[i].id);
		sprintf(arg[1],"%d", tasks[i].is_hard);
		sprintf(arg[2],"%llu", tasks[i].C);
		sprintf(arg[3],"%llu", tasks[i].T);
		sprintf(arg[4],"%llu", tasks[i].O);
		sprintf(arg[5],"%llu", time0);
		sprintf(arg[6],"%d", tasks[i].njobs);

		pid_tasks[i] = fork();
		if(pid_tasks[i] == 0) {
			execl("./task", task_name, arg[0], arg[1], arg[2], arg[3],
			      arg[4], arg[5], arg[6], NULL);
			printf("Error: execv: task\n");
			exit(0);
		}
	}

	printf("LAUNCH: Waiting ...\n");
	for(i = 0; i < ntasks; i++) {
		waitpid(0, &status, 0);
		if(WIFEXITED(status)) {
			printf("LAUNCH:task:%d: has finished\n", WEXITSTATUS(status));
		}
	}

	printf("LAUNCH: Disabling moker tracing\n");
	if((syscall(SYS_MOKER_TRACING_ENABLE, 0)) < 0){
		perror("ERROR: Disabling MOKER_TRACING failed");
		exit(-1);
	}

	printf("LAUNCH: Finishing ...\n");

	return 0;
}
