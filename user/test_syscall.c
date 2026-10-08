/*
 *  registers getpid, checks the wrapper runs, then deregisters it
 */

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>

#include "../throttle_ioctl.h"

int main(void) {

	int fd = open("/dev/throttler", O_RDWR);
	if (fd < 0) { perror("open"); return 1; }

	if (ioctl(fd, ADD_SYS, 39) < 0) perror("ADD_SYS 39");	//replace getpid
	printf("getpid returned %ld\n", syscall(SYS_getpid));	//should pass through the wrapper

	if (ioctl(fd, DEL_SYS, 39) < 0) perror("DEL_SYS 39");	//release getpid
	printf("getpid returned %ld\n", syscall(SYS_getpid));	//should NOT pass through the wrapper

	close(fd);
	return 0;
}
