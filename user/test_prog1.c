//test di prova: invoca N volte la syscall passata come argomento
//per verificare il filtro sul nome del programma (current->comm)

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

int main(int argc, char **argv) {
        int i, n, nr;
        if (argc < 3) { printf("uso: %s syscall-num num-chiamate\n", argv[0]); return 1; }
        nr = strtol(argv[1], NULL, 10);
        n  = strtol(argv[2], NULL, 10);
        for (i = 0; i < n; i++)
                syscall(nr);
        return 0;
}
