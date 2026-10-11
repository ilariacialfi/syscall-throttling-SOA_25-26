/*
 * @file registry.c
 * @brief configuration state of the syscall throttling module:
 *        registered syscall numbers, MAX, monitor on/off
 *        (locking scheme derived from dev_write in driver-concurrency.c;
 *        list of syscall numbers modeled on restore[] in work-queues.c;
 *        monitor flag modeled on enable_sleep in queuing-service.c)
 *
 * @author Ilaria Cialfi
 *
 * @date October 4, 2026
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/errno.h>
#include <asm/unistd.h>		/* For NR_syscalls */
#include <linux/string.h>
#include <linux/jiffies.h> 	/* for time windows */

#include "throttler.h"
#include "throttle_ioctl.h"

static DEFINE_MUTEX(registry_mutex);	//serializes every access to the configuration

static int enable_monitor = 0;		//enable/disable throttling monitor
static unsigned long max_calls = 0;	//MAX invocations per second

static int registered_sys[MAX_REGISTERED_SYSCALLS] = {[0 ... (MAX_REGISTERED_SYSCALLS-1)] -1};  //registered syscall numbers or -1 if free

struct euid_entry {
    int euid;                 // -1 = free slot (0 = root)
    unsigned long win_start;
    unsigned long count;
};

static struct euid_entry registered_euid[MAX_REGISTERED_EUIDS] = {[0 ... (MAX_REGISTERED_EUIDS-1)] = { .euid = -1}};       //registered EUIDs, -1 = free slot

struct prog_entry {
    char name[PROG_NAME_MAX]; // "" = free slot
    unsigned long win_start;
    unsigned long count;
};

static struct prog_entry registered_prog[MAX_REGISTERED_PROGS];	//registered program paths, empty string = free slot

int registry_add_syscall(int nr) {

	int i;
	int free_slot = -1;

	if (nr < 0 || nr >= NR_syscalls) 
		return -EINVAL;	//not an existing syscall
	if (nr == __NR_exit || nr == __NR_exit_group || nr == __NR_rt_sigreturn)
		return -EINVAL;	//case syscalls that never returns to the wrapper

	mutex_lock(&registry_mutex);
	for (i = 0; i < MAX_REGISTERED_SYSCALLS; i++) {
		if (registered_sys[i] == nr) {			//already registered
			mutex_unlock(&registry_mutex);
			return -EEXIST;
		}
		if (registered_sys[i] == -1 && free_slot == -1) free_slot = i;	//remember the first free slot
	}
	if (free_slot == -1) {					//no space left
		mutex_unlock(&registry_mutex);
		return -ENOSPC;
	}
	registered_sys[free_slot] = nr;
	mutex_unlock(&registry_mutex);

	printk("%s: syscall %d registered\n", MODNAME, nr);
	return 0;
}

int registry_del_syscall(int nr) {

	int i;

	if (nr < 0 || nr >= NR_syscalls) return -EINVAL;	//not an existing syscall

	mutex_lock(&registry_mutex);
	for (i = 0; i < MAX_REGISTERED_SYSCALLS; i++) {
		if (registered_sys[i] == nr) {
			registered_sys[i] = -1;			//slot is free again
			mutex_unlock(&registry_mutex);
			printk("%s: syscall %d deregistered\n", MODNAME, nr);
			return 0;
		}
	}
	mutex_unlock(&registry_mutex);

	return -ENOENT;						//was not registered
}

int registry_add_euid(int euid) {

	int i;
	int free_slot = -1;

	if (euid < 0) return -EINVAL;				//not a valid user ID

	mutex_lock(&registry_mutex);
	for (i = 0; i < MAX_REGISTERED_EUIDS; i++) {
		if (registered_euid[i].euid == euid) {		//already registered
			mutex_unlock(&registry_mutex);
			return -EEXIST;
		}
		if (registered_euid[i].euid == -1 && free_slot == -1) free_slot = i;	//remember the first free slot
	}
	if (free_slot == -1) {					//no space left
		mutex_unlock(&registry_mutex);
		return -ENOSPC;
	}
	registered_euid[free_slot].euid = euid;
	registered_euid[free_slot].count = 0;
	registered_euid[free_slot].win_start = jiffies;
	mutex_unlock(&registry_mutex);

	printk("%s: EUID %d registered\n", MODNAME, euid);
	return 0;
}

int registry_del_euid(int euid) {

	int i;

	if (euid < 0) return -EINVAL;				//not a valid user ID

	mutex_lock(&registry_mutex);
	for (i = 0; i < MAX_REGISTERED_EUIDS; i++) {
		if (registered_euid[i].euid == euid) {
			registered_euid[i].euid = -1;			//slot is free again
			mutex_unlock(&registry_mutex);
			printk("%s: EUID %d deregistered\n", MODNAME, euid);
			return 0;
		}
	}
	mutex_unlock(&registry_mutex);

	return -ENOENT;						//was not registered
}

int registry_add_prog(const char *name) {

	int i;
	int free_slot = -1;

	mutex_lock(&registry_mutex);
	for (i = 0; i < MAX_REGISTERED_PROGS; i++) {
		if (registered_prog[i].name[0] != '\0' && strcmp(registered_prog[i].name, name) == 0) {	//already registered
			mutex_unlock(&registry_mutex);
			return -EEXIST;
		}
		if (registered_prog[i].name[0] == '\0' && free_slot == -1) free_slot = i;	//remember the first free slot
	}
	if (free_slot == -1) {					//no space left
		mutex_unlock(&registry_mutex);
		return -ENOSPC;
	}
	memcpy(registered_prog[free_slot].name, name, strlen(name) + 1);	//+1 copies the terminator too
	registered_prog[free_slot].count = 0;
	registered_prog[free_slot].win_start = jiffies;
	mutex_unlock(&registry_mutex);

	printk("%s: program %s registered\n", MODNAME, name);
	return 0;
}

int registry_del_prog(const char *name) {

	int i;

	mutex_lock(&registry_mutex);
	for (i = 0; i < MAX_REGISTERED_PROGS; i++) {
		if (registered_prog[i].name[0] != '\0' && strcmp(registered_prog[i].name, name) == 0) {
			registered_prog[i].name[0] = '\0';		//slot is free again
			mutex_unlock(&registry_mutex);
			printk("%s: program %s deregistered\n", MODNAME, name);
			return 0;
		}
	}
	mutex_unlock(&registry_mutex);

	return -ENOENT;						//was not registered
}

void registry_set_max(unsigned long max) {

	mutex_lock(&registry_mutex);
	max_calls = max;
	mutex_unlock(&registry_mutex);

	printk("%s: MAX set to %lu\n", MODNAME, max);
}

void registry_set_monitor(int on) {

	mutex_lock(&registry_mutex);
	enable_monitor = on ? 1 : 0;
	mutex_unlock(&registry_mutex);

	printk("%s: monitor %s\n", MODNAME, on ? "on" : "off");
}

/* fills throttle_config: monitor, MAX, syscalls, EUIDs and program names. */
void registry_get_config(struct throttle_config *confg) {

	int i;

	mutex_lock(&registry_mutex);
	confg->monitor = enable_monitor;
	confg->max = max_calls;
	for (i = 0; i < MAX_REGISTERED_SYSCALLS; i++) confg->syscalls[i] = registered_sys[i];
	for (i = 0; i < MAX_REGISTERED_EUIDS; i++) confg->euids[i] = registered_euid[i].euid;
	for (i = 0; i < MAX_REGISTERED_PROGS; i++) memcpy(confg->progs[i], registered_prog[i].name, PROG_NAME_MAX);
	mutex_unlock(&registry_mutex);
}

//returns 1 if the monitor is on
int registry_monitor_on(void) {

	int on;

	mutex_lock(&registry_mutex);
	on = enable_monitor;
	mutex_unlock(&registry_mutex);
	return on;
}

//returns 1 if euid is registered
int registry_is_euid_registered(int euid) {

	int i, found = 0;

	mutex_lock(&registry_mutex);
	for (i = 0; i < MAX_REGISTERED_EUIDS; i++)
		if (registered_euid[i].euid == euid) { found = 1; break; }
	mutex_unlock(&registry_mutex);
	return found;
}

//returns 1 if program is registered
int registry_is_prog_registered(const char *name) {
	int i, found = 0;
	mutex_lock(&registry_mutex);
	for (i = 0; i < MAX_REGISTERED_PROGS; i++) {
		if (strcmp(registered_prog[i].name, "") == 0)
			continue;
		if (strcmp(registered_prog[i].name, name) == 0) {
			found = 1;
			break;
		}
	}
	mutex_unlock(&registry_mutex);
	return found;
}

/* window + count for one subject slot; call with registry_mutex held.
   Rolls the 1-second window if it is expired, counts syscall invocations,
   counts the left tick remaning until the windows ends.
   Returns SUBJ_UNDER or SUBJ_OVER. */
static int count_slot(unsigned long *win_start, unsigned long *count, unsigned long *left)
{
	unsigned long now = jiffies;

	if (time_after(now, *win_start + HZ)) {   // more than 1 s (HZ jiffies) since the window opened
		*win_start = now;                      // open a new window
		*count = 0;
	}
	
	*left = *win_start + HZ - now;
	
	if (*count < max_calls){
		(*count)++;
		return SUBJ_UNDER;
	} 
	return SUBJ_OVER;
}

int registry_count_euid(int euid, unsigned long *left)
{
	int i, res = SUBJ_NOT_REGISTERED;
	*left = 0;
	
	mutex_lock(&registry_mutex);
	for (i = 0; i < MAX_REGISTERED_EUIDS; i++) {
		if (registered_euid[i].euid == euid) {
			res = count_slot(&registered_euid[i].win_start, &registered_euid[i].count, left);
			break;
		}
	}
	mutex_unlock(&registry_mutex);
	return res;
}

int registry_count_prog(const char *name, unsigned long *left)
{
	int i, res = SUBJ_NOT_REGISTERED;
	*left = 0;

	mutex_lock(&registry_mutex);
	for (i = 0; i < MAX_REGISTERED_PROGS; i++) {
		if (registered_prog[i].name[0] != '\0' &&
				strcmp(registered_prog[i].name, name) == 0) {
			res = count_slot(&registered_prog[i].win_start, &registered_prog[i].count, left);
			break;
		}
	}
	mutex_unlock(&registry_mutex);
	return res;
}
