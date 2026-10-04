/*
 *  user program to test the device of the syscall throttling module:
 *  opens /dev/throttler, issues a dummy ioctl command and closes it
 */

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

int main(void) {

	int fd = open("/dev/throttler", O_RDWR);//goes to dev_open - requires root, the device file is rw only for its owner
	if (fd < 0) {
		perror("open");//device file missing, module not mounted or minor not allowed
		return 1;
	}

	ioctl(fd, 42, 7);//goes to dev_ioctl - dummy command (42) and parameter (7), just printed by the driver

	close(fd);//goes to dev_release
	return 0;
}
