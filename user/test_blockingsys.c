/* 
 * @file test_blockingsys.c 
 * @brief test program that performs N invocations 
 *        of a blocking system call.
 *        Choose: clock_nanosleep (syscall nr 230)
 *
 * @author Ilaria Cialfi 
 * 
 * @date October 13, 2026 
 */

#include <stdlib.h>
#include <unistd.h>
int main(int argc, char **argv) {
	if (argc < 2) return 1;
	int n = atoi(argv[1]);
	int i;
	for (i = 0; i < n; i++)
		sleep(1);	//libc sleep() -> clock_nanosleep (230) 
	return 0;
}
