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

#include "throttle_ioctl.h"

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
void registry_get_config(struct throttle_config *confg);
int registry_monitor_on(void);
int registry_is_euid_registered(int euid);
int registry_is_prog_registered(const char *path);
/* main.c */
extern unsigned long the_syscall_table;	//defined in main.c, set from the module parameter

/* sctable.c */
int sctable_replace(int nr);
void sctable_restore(int nr);
void sctable_restore_all(void);
#endif
