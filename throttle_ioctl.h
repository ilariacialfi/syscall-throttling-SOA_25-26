/*
 * @file throttle_ioctl.h
 * @brief ioctl commands of the syscall throttling module,
 *        shared by the module and the user-space programs
 *
 * @author Ilaria Cialfi
 *
 * @date October 6, 2026
 */

#ifndef _THROTTLE_IOCTL_H
#define _THROTTLE_IOCTL_H

#include <linux/ioctl.h>	/* For _IO */

/* plain numbers (as in the course example) collide with generic ioctl commands
   handled by the kernel before the driver: e.g. 2 is FIGETBSZ and never reaches
   dev_ioctl; _IO adds a magic letter to make the codes unique for this driver */
#define THROTTLE_MAGIC 't'

#define MAX_REGISTERED_SYSCALLS 20	//max number of syscalls that can be registered at the same time
#define MAX_REGISTERED_EUIDS 20		//max number of EUIDs that can be registered at the same time
#define MAX_REGISTERED_PROGS 20		//max number of programs that can be registered
#define PROG_NAME_MAX 16      //size of TASK_COMM_LEN kernel constant

#define ADD_SYS      _IO(THROTTLE_MAGIC, 1)	//param: syscall number
#define DEL_SYS      _IO(THROTTLE_MAGIC, 2)	//param: syscall number
#define SET_MAX      _IO(THROTTLE_MAGIC, 3)	//param: MAX invocations per second
#define MONITOR_ON   _IO(THROTTLE_MAGIC, 4)	//param: unused
#define MONITOR_OFF  _IO(THROTTLE_MAGIC, 5)	//param: unused
#define ADD_EUID     _IO(THROTTLE_MAGIC, 6)	//param: effective user ID
#define DEL_EUID     _IO(THROTTLE_MAGIC, 7)	//param: effective user ID
#define ADD_PROG     _IOW(THROTTLE_MAGIC, 8, char[PROG_NAME_MAX])	//param: address of a PROG_NAME_MAX buffer with the absolute path
#define DEL_PROG     _IOW(THROTTLE_MAGIC, 9, char[PROG_NAME_MAX])   	//param: address of a PROG_NAME_MAX buffer with the program name

struct throttle_config {
	int monitor;				//0 = off, 1 = on
	unsigned long max;			//MAX
	int syscalls[MAX_REGISTERED_SYSCALLS];	//-1 = free syscall slot
	int euids[MAX_REGISTERED_EUIDS];	//-1 = free EUID slot
	char progs[MAX_REGISTERED_PROGS][PROG_NAME_MAX];
	unsigned long peak_delay_ms;		//peak blocking delay, in milliseconds
	char peak_prog[PROG_NAME_MAX];		//program responsible for the peak ("" if none yet)
	int peak_euid;				//euid responsible for the peak (-1 if none yet)
};

#define GET_CONFIG   _IOR(THROTTLE_MAGIC, 10, struct throttle_config)	//read the current configuration

#endif
