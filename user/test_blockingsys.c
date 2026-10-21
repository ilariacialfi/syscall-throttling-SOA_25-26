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
#include <time.h>
#include <sys/syscall.h>

int main(int argc, char **argv) {
	if (argc < 2) return 1;
	int n = atoi(argv[1]);
	struct timespec ts = { 0, 50 * 1000 * 1000 };	//50 ms
	int i;
	for (i = 0; i < n; i++)
		syscall(SYS_clock_nanosleep, CLOCK_MONOTONIC, 0, &ts, NULL);	//syscall 230
	return 0;
}
