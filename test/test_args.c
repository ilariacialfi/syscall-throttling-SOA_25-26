/* 
 * @file test_args.c 
 * @brief test program that performs N invocations (argv[1]) 
 *        of a system call that takes arguments.
 *        Used: write (syscall nr 1)
 *
 * @author Ilaria Cialfi 
 * 
 * @date October 13, 2026 
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

int main(int argc, char **argv) {
	if (argc < 2) { 
		fprintf(stderr, "usage: %s <n-calls>\n", argv[0]);
		return 1;
	}
	int n = atoi(argv[1]);
	int i;
	for (i = 0; i < n; i++)
		write(1, "test\n", 5);	//syscall 1: fd, buf, count
	return 0;
}
