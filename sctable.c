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
#include <linux/errno.h>

#include "throttler.h"
#include "lib/include/scth.h"

typedef long (*syscall_ptr_t)(const struct pt_regs *);	//single-argument syscall signature (kernel >= 4.17)

static int replaced_nr[MAX_REGISTERED_SYSCALLS] = {[0 ... (MAX_REGISTERED_SYSCALLS-1)] -1};	//syscall numbers currently replaced, -1 = free
static syscall_ptr_t original_syscalls[MAX_REGISTERED_SYSCALLS];	//original syscall pointers, to call and to restore

/* returns the entry where syscall nr is stored */
static int sc_slot(int nr) {
	int i;
	for (i = 0; i < MAX_REGISTERED_SYSCALLS; i++)
		if (replaced_nr[i] == nr) return i;
	return -1;
}

/* the wrapper installed in place of the real syscall: for now it just calls
   the original one and returns its result, so behavior is unchanged */
static long my_wrapper(const struct pt_regs *regs) {

	int nr = (int)regs->orig_ax;	//on x86-64 orig_ax holds the syscall number
	int slot = sc_slot(nr);

	printk("%s: wrapper called for syscall %d by thread %d\n", MODNAME, nr, current->pid);

	if (slot < 0) return -ENOSYS; //should not happen
	return original_syscalls[slot](regs);
}

int sctable_replace(int nr){
	unsigned long *table = (unsigned long *)the_syscall_table;
	int i, free_slot = -1;

	if (sc_slot(nr) >= 0) return 0;	//already replaced

	for (i = 0; i< MAX_REGISTERED_SYSCALLS; i++)
		if (replaced_nr[i] == -1) { free_slot = i; break; }
	if (free_slot == -1) return -ENOSPC;

	original_syscalls[free_slot] = (syscall_ptr_t)table[nr];	//save the original pointer
	replaced_nr[free_slot] = nr;

	unprotect_memory();
	table[nr] = (unsigned long)my_wrapper;		//install the wrapper
	protect_memory();

	printk("%s: syscall %d replaced\n", MODNAME, nr);
	return 0;
}

void sctable_restore(int nr) {

	unsigned long *table = (unsigned long *)the_syscall_table;
	int slot = sc_slot(nr);

	if (slot < 0) return;			//nothing to restore

	unprotect_memory();
	table[nr] = (unsigned long)original_syscalls[slot];	//put the original back
	protect_memory();

	replaced_nr[slot] = -1; //slot is free again

	printk("%s: syscall %d restored\n", MODNAME, nr);
}

void sctable_restore_all(void) {
		int i;
		for (i = 0; i < MAX_REGISTERED_SYSCALLS; i++)
			if (replaced_nr[i] != -1) sctable_restore(replaced_nr[i]);
}
