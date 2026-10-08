/*
 * @file main.c
 * @brief entry point of the syscall throttling module: init_module and
 *        cleanup_module, which start up and shut down the module components
 *        (currently the char device in device.c)
 *
 * @author Ilaria Cialfi
 *
 * @date October 4, 2026
 */


#include <linux/module.h>
#include <linux/kernel.h>

#include "throttler.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ilaria Cialfi");
MODULE_DESCRIPTION("syscall throttling");

unsigned long the_syscall_table = 0x0;
module_param(the_syscall_table, ulong, 0660);

int init_module(void) {

	if (the_syscall_table == 0x0){
		printk("%s: cannot manage sys_call_table address set to 0x0\n", MODNAME);
		return -1;
	}
	if (sctable_replace(39) != 0) return -1;	//getpid, test syscall
	return dev_register();
}

void cleanup_module(void) {
	sctable_restore();
	dev_unregister();
}

