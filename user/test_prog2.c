#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>

int main(void) {
        printf("getpid returned %ld\n", syscall(SYS_getpid));
        return 0;
}
