/*
 * @file device.c
 * @brief char device of the syscall throttling module: single minor,
 *        control interface based on ioctl only
 *        (derived from the course example driver-concurrency.c)
 *
 * @author Ilaria Cialfi
 *
 * @date October 4, 2026
 */

#define EXPORT_SYMTAB
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/pid.h>		/* For pid types */
#include <linux/tty.h>		/* For the tty declarations */
#include <linux/version.h>	/* For LINUX_VERSION_CODE */
#include <linux/uaccess.h>
#include <linux/mm.h>
#include <linux/slab.h>

#include "throttler.h"
#include "throttle_ioctl.h"

static int dev_open(struct inode *, struct file *);
static int dev_release(struct inode *, struct file *);
static long dev_ioctl(struct file *, unsigned int, unsigned long);	

#define DEVICE_NAME "throttler"  /* Device file name in /dev/ */

static int Major;            /* Major number assigned to the device driver */

/* extract major and minor number of the device file from an open session;
   the path to the inode depends on the kernel version (f_inode since 4.0) */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 0, 0)
#define get_major(session)	MAJOR(session->f_inode->i_rdev)
#define get_minor(session)	MINOR(session->f_inode->i_rdev)
#else
#define get_major(session)	MAJOR(session->f_dentry->d_inode->i_rdev)
#define get_minor(session)	MINOR(session->f_dentry->d_inode->i_rdev)
#endif

#define MINORS 1	// a single I/O node is enough

/* the actual driver */

static int dev_open(struct inode *inode, struct file *file) {

   int minor;
   minor = get_minor(file);

   if(minor >= MINORS){
	return -ENODEV;
   }

   printk("%s: device file successfully opened for object with minor %d\n",MODNAME,minor);
//device opened by a default nop
   return 0;

}

static int dev_release(struct inode *inode, struct file *file) {

   printk("%s: device file closed\n",MODNAME);
//device closed by default nop
   return 0;

}

/* copies the program path from user space into a temporary kernel page and
   passes it to the registry */
static long prog_command(unsigned int command, unsigned long param) {

	char *path;
	unsigned long ret;
	long res;

	path = (char*)get_zeroed_page(GFP_KERNEL);
	if (path == NULL) return -ENOMEM;

	ret = copy_from_user(path, (char*)param, PROG_PATH_MAX);	//returns the number of bytes NOT copied
	if (ret != 0) {
		free_pages((unsigned long)path, 0);
		return -EFAULT;						//bad user buffer
	}
	path[PROG_PATH_MAX - 1] = '\0';					//make sure the string is terminated

	if (command == ADD_PROG) res = registry_add_prog(path);
	else res = registry_del_prog(path);

	free_pages((unsigned long)path, 0);
	return res;
}

/* entry point of the configuration commands: invoked by the kernel when a
   user program calls ioctl() on /dev/throttler; 'command' selects the
   operation, 'param' carries its argument (a value or a user-space address);
   the return value goes back to the caller of ioctl() */
static long dev_ioctl(struct file *filp, unsigned int command, unsigned long param) {

  printk("%s: somebody called an ioctl on dev with [major,minor] number [%d,%d], command %u and parameter %lu \n",MODNAME,get_major(filp),get_minor(filp),command,param);

  //read command: open to everyone
  if (command == GET_CONFIG) {
	struct throttle_config confg;
	registry_get_config(&confg);
	if (copy_to_user((void*)param, &confg, sizeof(confg))) return -EFAULT;
	return 0;
  }

  //read command: open to everyone
  if (command == GET_PROGS) {
	struct throttle_progs *p = kmalloc(sizeof(*p), GFP_KERNEL);
	if (p == NULL) return -ENOMEM;
	registry_get_progs(p);
	if (copy_to_user((void*)param, p, sizeof(*p))) { kfree(p); return -EFAULT; }
	kfree(p);
	return 0;
  }

  if (current->cred->euid.val != 0) return -EPERM;	//all current commands update the configuration: only EUID 0 (root) 
  
  //dispatch of the commands 
  switch (command) {
  case ADD_SYS:	{	//add syscall, param = syscall number
	int nr = (int)param;
	int ret = registry_add_syscall(nr);
	if (ret != 0) return ret;
	ret = sctable_replace(nr);
	if (ret != 0) registry_del_syscall(nr);	//rollback the registry if replace failed
	return ret;
		}
  case DEL_SYS:	{	//del syscall, param = syscall number
	int nr = (int)param;
	int ret = registry_del_syscall(nr);
	if (ret == 0) sctable_restore(nr);
	return ret;
		}
  case SET_MAX:		//set MAX, param = max invocations per second
	registry_set_max(param);
	return 0;
  case MONITOR_ON:		//monitor on
	registry_set_monitor(1);
	return 0;
  case MONITOR_OFF:		//monitor off
	registry_set_monitor(0);
	return 0;
  case ADD_EUID:	//add EUID, param = effective user ID
	return registry_add_euid((int)param);
  case DEL_EUID:	//del EUID, param = effective user ID
	return registry_del_euid((int)param);
  case ADD_PROG:	//add program, param = address of the path buffer
  	return prog_command(command, param);
  case DEL_PROG:	//del program, param = address of the path buffer
	return prog_command(command, param);
  default:
	return -EINVAL;	//unknown command
  }
}

/* operations supported by the device: maps open, close (release) and ioctl
   on /dev/throttler to the driver functions; read and write are not provided,
   so the kernel rejects them; .owner prevents unloading the module while
   the device file is open */
static struct file_operations fops = {
  .owner = THIS_MODULE,//do not forget this
  .open =  dev_open,
  .release = dev_release,
  .unlocked_ioctl = dev_ioctl
};

/* called by init_module (main.c): contains what the course example
   driver-concurrency.c does in its init_module - registers the char device
   asking the kernel for a free major number and binds it to fops;
   returns 0 on success, a negative error code otherwise */
int dev_register(void) {

	Major = __register_chrdev(0, 0, 256, DEVICE_NAME, &fops);
	//actually allowed minors are directly controlled within this driver

	if (Major < 0) {
	  printk("%s: registering device failed\n",MODNAME);
	  return Major;
	}

	printk(KERN_INFO "%s: new device registered, it is assigned major number %d\n",MODNAME, Major);

	return 0;
}

/* called by cleanup_module (main.c): contains what the course example
   driver-concurrency.c does in its cleanup_module - releases the major
   number, so the device files no longer reach this driver */
void dev_unregister(void) {

	unregister_chrdev(Major, DEVICE_NAME);

	printk(KERN_INFO "%s: new device unregistered, it was assigned major number %d\n",MODNAME, Major);

	return;
}
