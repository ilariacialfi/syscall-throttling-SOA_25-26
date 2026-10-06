/*
 *  user program to test the device of the syscall throttling module:
 *  opens /dev/throttler, issues a dummy ioctl command and closes it
 */

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "../throttle_ioctl.h"

//issues one command and prints the result (perror translates the error code returned by the module)
void try(int fd, unsigned int cmd, unsigned long param, const char *what) {
	int ret = ioctl(fd, cmd, param);
	if (ret < 0) perror(what);
	else printf("%s: ok\n", what);
}

int main(void) {

	char *device = "/dev/throttler";
	int fd;

	fd = open(device, O_RDONLY); //read-only open is allowed to everyone: write permission is checked by the driver (EUID 0)
	if (fd == -1) {
		printf("open error on device %s\n", device);	//as in driver-concurrency/user/user.c
		return -1;
	}
	printf("device %s successfully opened\n", device);

	try(fd, ADD_SYS, 1, "add syscall 1");
	try(fd, ADD_SYS, 1, "add syscall 1 again");	//expected: File exists
	try(fd, DEL_SYS, 39, "del syscall 39");	//expected: No such file or directory
	try(fd, ADD_SYS, 39, "add syscall 39");
	try(fd, DEL_SYS, 39, "del syscall 39");
	try(fd, SET_MAX, 5, "set MAX 5");
	try(fd, MONITOR_ON, 0, "monitor on");
	try(fd, 99, 0, "unknown command");	//expected: Invalid argument

	close(fd);
	return 0;
}
