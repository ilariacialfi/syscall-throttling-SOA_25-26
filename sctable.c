/*
 * @file sctable.c
 * @brief replaces syscall table entrie at runtime  with a wrapper and restores it
 *
 * @author Ilaria Cialfi
 *
 * @date October 7, 2026
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/syscalls.h>
#include <linux/ptrace.h>	/* For struct pt_regs */

#include "throttler.h"
#include "lib/include/scth.h"

typedef long (*syscall_ptr_t)(const struct pt_regs *);	//single-argument syscall signature (kernel >= 4.17)

static int replaced_nr = -1;			//syscall number currently replaced, -1 = none
static syscall_ptr_t original_syscall = NULL;	//original pointer, to call and to restore

/* the wrapper installed in place of the real syscall: for now it just calls
   the original one and returns its result, so behavior is unchanged */
static long my_wrapper(const struct pt_regs *regs) {

	printk("%s: wrapper called for syscall %d by thread %d\n", MODNAME, replaced_nr, current->pid);

	return original_syscall(regs);
}

int sctable_replace(int nr){
	unsigned long *table = (unsigned long *)the_syscall_table;

	original_syscall = (syscall_ptr_t)table[nr];	//save the original pointer
	replaced_nr = nr;

	unprotect_memory();
	table[nr] = (unsigned long)my_wrapper;		//install the wrapper
	protect_memory();

	printk("%s: syscall %d replaced\n", MODNAME, nr);
	return 0;
}

void sctable_restore(void) {

	unsigned long *table = (unsigned long *)the_syscall_table;

	if (replaced_nr < 0) return;			//nothing to restore

	unprotect_memory();
	table[replaced_nr] = (unsigned long)original_syscall;	//put the original back
	protect_memory();

	printk("%s: syscall %d restored\n", MODNAME, replaced_nr);
	replaced_nr = -1;
}
