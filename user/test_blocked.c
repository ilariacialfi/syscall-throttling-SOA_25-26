/*
 * @file test_blocked.c
 * @brief spawns several concurrent processes that all invoke the same syscall,
 *        so that more than one thread is blocked at the same time: used to
 *        exercise the peak/average blocked-threads statistic
 *
 * @author Ilaria Cialfi
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/syscall.h>

int main(int argc, char **argv) {

	int nr, nproc, calls, i, j;

	if (argc != 4) {
		fprintf(stderr, "usage: %s <syscall_nr> <n_processes> <calls_each>\n", argv[0]);
		return 1;
	}
	nr    = atoi(argv[1]);
	nproc = atoi(argv[2]);
	calls = atoi(argv[3]);

	for (i = 0; i < nproc; i++) {
		pid_t pid = fork();
		if (pid < 0) { perror("fork"); return 1; }
		if (pid == 0) {			//child process
			for (j = 0; j < calls; j++)
				syscall(nr);	//invoke the registered syscall
			_exit(0);
		}
	}

	for (i = 0; i < nproc; i++)
		wait(NULL);			//parent waits for all children

	printf("done: %d processes x %d calls on syscall %d\n", nproc, calls, nr);
	return 0;
}
