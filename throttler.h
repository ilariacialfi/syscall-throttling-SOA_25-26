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

/* device.c */
int dev_register(void);
void dev_unregister(void);

/* registry.c */
int registry_add_syscall(int nr);
int registry_del_syscall(int nr);
int registry_add_euid(int euid);
int registry_del_euid(int euid);
void registry_set_max(unsigned long max);
void registry_set_monitor(int on);

#endif
