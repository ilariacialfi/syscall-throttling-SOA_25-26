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

#include "throttler.h"
#include "throttle_ioctl.h"

static int registered_euid[MAX_REGISTERED_EUIDS] = {[0 ... (MAX_REGISTERED_EUIDS-1)] -1};	//registered EUIDs, -1 = free slot
												
static DEFINE_MUTEX(registry_mutex);	//serializes every access to the configuration

static int enable_monitor = 0;		//enable/disable throttling monitor
static unsigned long max_calls = 0;	//MAX invocations per second
static int registered_sys[MAX_REGISTERED_SYSCALLS] = {[0 ... (MAX_REGISTERED_SYSCALLS-1)] -1};	//registered syscall numbers or -1 if free
static char registered_prog[MAX_REGISTERED_PROGS][PROG_PATH_MAX];	//registered program paths, empty string = free slot

int registry_add_syscall(int nr) {

	int i;
	int free_slot = -1;

	if (nr < 0 || nr >= NR_syscalls) return -EINVAL;	//not an existing syscall

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
		if (registered_euid[i] == euid) {		//already registered
			mutex_unlock(&registry_mutex);
			return -EEXIST;
		}
		if (registered_euid[i] == -1 && free_slot == -1) free_slot = i;	//remember the first free slot
	}
	if (free_slot == -1) {					//no space left
		mutex_unlock(&registry_mutex);
		return -ENOSPC;
	}
	registered_euid[free_slot] = euid;
	mutex_unlock(&registry_mutex);

	printk("%s: EUID %d registered\n", MODNAME, euid);
	return 0;
}

int registry_del_euid(int euid) {

	int i;

	if (euid < 0) return -EINVAL;				//not a valid user ID

	mutex_lock(&registry_mutex);
	for (i = 0; i < MAX_REGISTERED_EUIDS; i++) {
		if (registered_euid[i] == euid) {
			registered_euid[i] = -1;			//slot is free again
			mutex_unlock(&registry_mutex);
			printk("%s: EUID %d deregistered\n", MODNAME, euid);
			return 0;
		}
	}
	mutex_unlock(&registry_mutex);

	return -ENOENT;						//was not registered
}

int registry_add_prog(const char *path) {

	int i;
	int free_slot = -1;

	if (path[0] != '/') return -EINVAL;			//an absolute path is required

	mutex_lock(&registry_mutex);
	for (i = 0; i < MAX_REGISTERED_PROGS; i++) {
		if (registered_prog[i][0] != '\0' && strcmp(registered_prog[i], path) == 0) {	//already registered
			mutex_unlock(&registry_mutex);
			return -EEXIST;
		}
		if (registered_prog[i][0] == '\0' && free_slot == -1) free_slot = i;	//remember the first free slot
	}
	if (free_slot == -1) {					//no space left
		mutex_unlock(&registry_mutex);
		return -ENOSPC;
	}
	memcpy(registered_prog[free_slot], path, strlen(path) + 1);	//+1 copies the terminator too
	mutex_unlock(&registry_mutex);

	printk("%s: program %s registered\n", MODNAME, path);
	return 0;
}

int registry_del_prog(const char *path) {

	int i;

	mutex_lock(&registry_mutex);
	for (i = 0; i < MAX_REGISTERED_PROGS; i++) {
		if (registered_prog[i][0] != '\0' && strcmp(registered_prog[i], path) == 0) {
			registered_prog[i][0] = '\0';		//slot is free again
			mutex_unlock(&registry_mutex);
			printk("%s: program %s deregistered\n", MODNAME, path);
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

void registry_get_config(struct throttle_config *confg) {

	int i;

	mutex_lock(&registry_mutex);
	confg->monitor = enable_monitor;
	confg->max = max_calls;
	for (i = 0; i < MAX_REGISTERED_SYSCALLS; i++) confg->syscalls[i] = registered_sys[i];
	for (i = 0; i < MAX_REGISTERED_EUIDS; i++) confg->euids[i] = registered_euid[i];
	mutex_unlock(&registry_mutex);
}

void registry_get_progs(struct throttle_progs *p) {

	int i;

	mutex_lock(&registry_mutex);
	for (i = 0; i < MAX_REGISTERED_PROGS; i++)
		memcpy(p->progs[i], registered_prog[i], PROG_PATH_MAX);
	mutex_unlock(&registry_mutex);
}
