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

#define ADD_SYS      _IO(THROTTLE_MAGIC, 1)	//param: syscall number
#define DEL_SYS      _IO(THROTTLE_MAGIC, 2)	//param: syscall number
#define SET_MAX      _IO(THROTTLE_MAGIC, 3)	//param: MAX invocations per second
#define MONITOR_ON   _IO(THROTTLE_MAGIC, 4)	//param: unused
#define MONITOR_OFF  _IO(THROTTLE_MAGIC, 5)	//param: unused
#define ADD_EUID     _IO(THROTTLE_MAGIC, 6)	//param: effective user ID
#define DEL_EUID     _IO(THROTTLE_MAGIC, 7)	//param: effective user ID
						//
#endif
