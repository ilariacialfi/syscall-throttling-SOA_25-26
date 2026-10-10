/*
 * @file throttlectl.c
 * @brief user level command-line tool to configure the syscall throttling module:
 *        opens /dev/throttler and sends the ioctl command chosen from the arguments
 *
 * @author Ilaria Cialfi
 *
 * @date October 8, 2026
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "../throttle_ioctl.h"

static void usage(const char *prog) {
	fprintf(stderr,
		"uso: %s <comando> [argomento]\n"
		"  add-sys <n>      registra la syscall n\n"
		"  del-sys <n>      deregistra la syscall n\n"
		"  add-uid <id>     registra l'EUID id\n"
		"  del-uid <id>     deregistra l'EUID id\n"
		"  add-prog <path>  registra il programma (nome del programma)\n"
		"  del-prog <path>  deregistra il programma\n"
		"  set-max <n>      imposta MAX (invocazioni al secondo)\n"
		"  on               accende il monitor\n"
		"  off              spegne il monitor\n"
		"  show             mostra la configurazione corrente\n", prog);
}

int main(int argc, char **argv) {

	int fd, ret;
	char path[PROG_PATH_MAX];

	if (argc < 2) { usage(argv[0]); return 1; }

	fd = open("/dev/throttler", O_RDONLY);	//read-only: everyone can open; writes are checked by the driver
	if (fd < 0) { perror("open /dev/throttler"); return 1; }

	//strcmp returns 0 if strings are equal
	if (strcmp(argv[1], "add-sys") == 0 && argc == 3)
		ret = ioctl(fd, ADD_SYS, atoi(argv[2]));
	else if (strcmp(argv[1], "del-sys") == 0 && argc == 3)
		ret = ioctl(fd, DEL_SYS, atoi(argv[2]));
	else if (strcmp(argv[1], "add-uid") == 0 && argc == 3)
		ret = ioctl(fd, ADD_EUID, atoi(argv[2]));
	else if (strcmp(argv[1], "del-uid") == 0 && argc == 3)
		ret = ioctl(fd, DEL_EUID, atoi(argv[2]));
	else if (strcmp(argv[1], "add-prog") == 0 && argc == 3) {
		strncpy(path, argv[2], PROG_PATH_MAX);
		path[PROG_PATH_MAX - 1] = '\0';
		ret = ioctl(fd, ADD_PROG, (unsigned long)path);
	}
	else if (strcmp(argv[1], "del-prog") == 0 && argc == 3) {
		strncpy(path, argv[2], PROG_PATH_MAX);
		path[PROG_PATH_MAX - 1] = '\0';
		ret = ioctl(fd, DEL_PROG, (unsigned long)path);
	}
	else if (strcmp(argv[1], "set-max") == 0 && argc == 3)
		ret = ioctl(fd, SET_MAX, atol(argv[2]));
	else if (strcmp(argv[1], "on") == 0 && argc == 2)
		ret = ioctl(fd, MONITOR_ON, 0);
	else if (strcmp(argv[1], "off") == 0 && argc == 2)
		ret = ioctl(fd, MONITOR_OFF, 0);
	else if (strcmp(argv[1], "show") == 0 && argc == 2) {
		struct throttle_config confg;
		int i;
		if (ioctl(fd, GET_CONFIG, (unsigned long)&confg) < 0) { perror("show"); close(fd); return 1; }
		if (confg.monitor)
			printf("monitor: on\n");
	        else
			printf("monitor: off\n");
		printf("MAX: %lu\n", confg.max);
		printf("syscall registered:");
		for (i = 0; i < MAX_REGISTERED_SYSCALLS; i++)
			if (confg.syscalls[i] != -1) printf(" %d", confg.syscalls[i]);
		printf("\nEUID registered:");
		for (i = 0; i < MAX_REGISTERED_EUIDS; i++)
			if (confg.euids[i] != -1) printf(" %d", confg.euids[i]);
		printf("\n");
		struct throttle_progs *p = malloc(sizeof(*p));
		if (p == NULL) { perror("malloc"); close(fd); return 1; }
		if (ioctl(fd, GET_PROGS, (unsigned long)p) < 0) { perror("show progs"); free(p); close(fd); return 1; }
		printf("programmi registrati:");
		for (i = 0; i < MAX_REGISTERED_PROGS; i++)
			if (p->progs[i][0] != '\0') printf(" %s", p->progs[i]);
		printf("\n");
		free(p);
		close(fd);
		return 0;
	}
	else { usage(argv[0]); close(fd); return 1; }

	if (ret < 0) perror(argv[1]);
	else printf("%s: ok\n", argv[1]);

	close(fd);
	if (ret < 0)
		return 1;
	else
		return 0;
}
