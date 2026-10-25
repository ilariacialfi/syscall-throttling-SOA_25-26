/*
 * @file sctable.c
 * @brief hooks the sys_call_table: installs a generic wrapper in place of a
 *        registered syscall and restores the original pointer. The wrapper
 *        enforces the throttling (per-subject count, blocking over MAX) and
 *        feeds the statistics; sctable_shutdown performs the safe removal
 *        (drain of in-flight calls + table restore).
 *        (blocking modeled on the wait queue of queuing-service.c)
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
#include <linux/wait.h>
#include <linux/sched.h>	/* For schedule() */
#include <linux/ktime.h>

#include "throttler.h"
#include "lib/include/scth.h"

static DECLARE_WAIT_QUEUE_HEAD(throttle_queue);
static atomic_t active_calls = ATOMIC_INIT(0); 	//threads currently inside the wrapper
static int unloading = 0;			//set during cleanup_module to reset blocked threads

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

/* the wrapper installed in place of the real syscall registered: when the monitor is on,
   it counts the invocation per registered subject (EUID and/or program) in the
   1-second window; if a subject is over MAX it blocks the thread for the
   rest of the window and records the blocking delay for the statistics; 
   then it calls the original syscall and returns its result. 
   A signal received while blocked aborts without running the original syscall. */
static long my_wrapper(const struct pt_regs *regs) {

	int nr = (int)regs->orig_ax;	//on x86-64 orig_ax holds the syscall number
	int slot = sc_slot(nr);

	syscall_ptr_t orig;
	long ret_code;

	if (slot < 0) return -ENOSYS;
	orig = original_syscalls[slot];

	atomic_inc(&active_calls); 	//entering the wrapper

	if (registry_monitor_on()) {
		unsigned long left_euid = 0;
		unsigned long left_prog = 0;
		int result_euid = registry_count_euid(current->cred->euid.val, &left_euid);
		int result_prog = registry_count_prog(current->comm, &left_prog);

		/* over MAX logging & block*/    
		if (result_euid == SUBJ_OVER || result_prog == SUBJ_OVER) {
			ktime_t block_start = ktime_get();	//start of blocked time

			if (result_euid == SUBJ_OVER && result_prog == SUBJ_OVER)
				printk("%s: syscall %d OVER MAX - euid %d and program %s, thread %d\n", 
						MODNAME, nr, current->cred->euid.val, current->comm, current->pid);
			else if (result_euid == SUBJ_OVER)
				printk("%s: syscall %d OVER MAX - euid %d, thread %d\n", 
						MODNAME, nr, current->cred->euid.val, current->pid);
			else if (result_prog == SUBJ_OVER)
				printk("%s: syscall %d OVER MAX - program %s, thread %d\n", 
						MODNAME, nr, current->comm, current->pid);
			
			registry_block_enter();		//this thread is now blocked

			while (!unloading && (result_euid == SUBJ_OVER || result_prog == SUBJ_OVER)) {
				unsigned long wait = 0;
				if (result_euid == SUBJ_OVER && left_euid > wait) wait = left_euid;
				if (result_prog == SUBJ_OVER && left_prog > wait) wait = left_prog;

				/* wait for "wait" jiffies*/
				long ret = wait_event_interruptible_timeout(throttle_queue, unloading, wait);

				if (ret == -ERESTARTSYS){       	// a signal arrived while sleeping
					printk("%s: syscall %d interrupted by signal, thread %d\n",
							MODNAME, nr, current->pid);
					registry_block_exit();		//leaving the blocked state
					atomic_dec(&active_calls);	//leaving on signal
					return -ERESTARTSYS;    // do NOT run the original syscall
				}

				if (!registry_monitor_on())    	// monitor turned off while blocked -> let go every thread in queue
					break;
				result_euid = registry_count_euid(current->cred->euid.val, &left_euid);
				result_prog = registry_count_prog(current->comm, &left_prog);
			}
			registry_block_exit();		//leaving the blocked state
			registry_record_delay(ktime_to_ns(ktime_sub(ktime_get(), block_start)), current->cred->euid.val, current->comm); //end of blocked time
		} 

		/* observed logging */
		if (result_euid != SUBJ_NOT_REGISTERED && result_prog != SUBJ_NOT_REGISTERED) 
			printk("%s: syscall %d observed - euid %d and program %s, thread %d\n",
					MODNAME, nr, current->cred->euid.val, current->comm, current->pid);
		else if (result_euid != SUBJ_NOT_REGISTERED)
			printk("%s: syscall %d observed - euid %d, thread %d\n",
					MODNAME, nr, current->cred->euid.val, current->pid);
		else if (result_prog != SUBJ_NOT_REGISTERED)
			printk("%s: syscall %d observed - program %s, thread %d\n",
					MODNAME, nr, current->comm, current->pid);
	}
	ret_code = orig(regs);
	atomic_dec(&active_calls);
	return ret_code;
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
	table[nr] = (unsigned long)my_wrapper;	//install the wrapper
	protect_memory();

	printk("%s: syscall %d replaced\n", MODNAME, nr);
	return 0;
}

void sctable_restore(int nr) {

	unsigned long *table = (unsigned long *)the_syscall_table;
	int slot = sc_slot(nr);

	if (slot < 0) return;	//nothing to restore

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

/* called from cleanup_module: stop accepting calls, release the blocked
   threads, restore the table and wait until no thread is inside the wrapper */
void sctable_shutdown(void) {
        unloading = 1;                          // tell blocked threads to stop waiting
        wake_up_interruptible(&throttle_queue); // wake them so they re-check unloading
        sctable_restore_all();                  // from now on no new call enters the wrapper
        while (atomic_read(&active_calls) > 0)  // wait for the in-flight calls to finish
                schedule();                     // leave the CPU, then re-check
}
