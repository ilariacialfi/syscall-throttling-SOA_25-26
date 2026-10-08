/*
 * @file throttler.h
 * @brief internal header of the syscall throttling module: module name and
 *        declarations of the functions shared among the source files
 *
 * @author Ilaria Cialfi
 *
 * @date October 5, 2026
 */


#ifndef _THROTTLER_H
#define _THROTTLER_H

#define MODNAME "THROTTLER"

#define MAX_REGISTERED_SYSCALLS 20	//max number of syscalls that can be registered at the same time
#define MAX_REGISTERED_EUIDS 20		//max number of EUIDs that can be registered at the same time
#define MAX_REGISTERED_PROGS 20		//max number of programs that can be registered at the same time

/* device.c */
int dev_register(void);
void dev_unregister(void);

/* registry.c */
int registry_add_syscall(int nr);
int registry_del_syscall(int nr);
int registry_add_euid(int euid);
int registry_del_euid(int euid);
int registry_add_prog(const char *path);
int registry_del_prog(const char *path);
void registry_set_max(unsigned long max);
void registry_set_monitor(int on);

/* main.c */
extern unsigned long the_syscall_table;	//defined in main.c, set from the module parameter

/* sctable.c */
int sctable_replace(int nr);
void sctable_restore(void);
#endif
