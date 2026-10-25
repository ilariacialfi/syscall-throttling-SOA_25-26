/* 
 * @file test_syscall_by_nr.c 
 * @brief test program that performs N invocations (argv[2]) 
 *        of a choosen system call in argv[1] using nr, 
 *        no matter of syscall arguments. 
 *
 * @author Ilaria Cialfi 
 * 
 * @date October 13, 2026 
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

int main(int argc, char **argv) {
        int i, n, nr;
        if (argc < 3) { 
	        fprintf(stderr, "usage: %s <syscall-nr> <n-calls>\n", argv[0]); 
	        return 1; 
	    }
        nr = strtol(argv[1], NULL, 10);
        n  = strtol(argv[2], NULL, 10);
        for (i = 0; i < n; i++)
                syscall(nr);
        return 0;
}
