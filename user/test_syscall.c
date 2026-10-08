/*
 *  calls getpid() a few times to check the syscall table replacement
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>

int main(void) {

	int i;
	for (i = 0; i < 3; i++)
		printf("getpid returned %ld\n", syscall(SYS_getpid));

	return 0;
}
