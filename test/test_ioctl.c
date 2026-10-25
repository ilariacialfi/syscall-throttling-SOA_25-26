/*
 * @file test_ioctl.c
 * @brief exercises every ioctl command and its error paths (double
 *        registration, deregistration of a missing entry, unknown command,
 *        out-of-range and non-returning syscall numbers)
 *
 * @author Ilaria Cialfi
 *
 * @date October 13, 2026
 */

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>
#include "../throttle_ioctl.h"

char prog[PROG_NAME_MAX];	//fixed-size buffer passed to the module

//issues one command and prints the result (perror translates the module's error code)
void try(int fd, unsigned int cmd, unsigned long param, const char *what) {
	int ret = ioctl(fd, cmd, param);
	if (ret < 0) perror(what);
	else printf("%s: ok\n", what);
}

int main(void) {

	char *device = "/dev/throttler";
	int fd;

	fd = open(device, O_RDONLY);	//read-only open allowed to everyone; writes checked by the driver (EUID 0)
	if (fd == -1) {
		perror(device); //returns error
		return 1;
	}
	printf("device %s successfully opened\n", device);

	/* syscalls */
	try(fd, ADD_SYS, 39, "add syscall 39");
	try(fd, ADD_SYS, 39, "add syscall 39 again");			//expected: File exists
	try(fd, DEL_SYS, 1, "del syscall 1 (not registered)");		//expected: No such file or directory
	try(fd, ADD_SYS, 999999, "add syscall 999999 (out of range)");	//expected: Invalid argument
	try(fd, ADD_SYS, 60, "add syscall 60 (exit, never returns)");	//expected: Invalid argument
	try(fd, DEL_SYS, 39, "del syscall 39");

	/* unknown command */
	try(fd, 99, 0, "unknown command");				//expected: Invalid argument

	/* EUIDs */
	try(fd, ADD_EUID, 1000, "add EUID 1000");
	try(fd, ADD_EUID, 1000, "add EUID 1000 again");			//expected: File exists
	try(fd, DEL_EUID, 1001, "del EUID 1001 (not registered)");	//expected: No such file or directory
	try(fd, DEL_EUID, 1000, "del EUID 1000");

	/* programs (short names, current->comm style) */
	strcpy(prog, "test_args");
	try(fd, ADD_PROG, (unsigned long)prog, "add program test_args");
	try(fd, ADD_PROG, (unsigned long)prog, "add program test_args again");	//expected: File exists
	strcpy(prog, "sleep");
	try(fd, DEL_PROG, (unsigned long)prog, "del program sleep (not registered)");	//expected: No such file or directory
	strcpy(prog, "test_args");
	try(fd, DEL_PROG, (unsigned long)prog, "del program test_args");

	close(fd);
	return 0;
}
