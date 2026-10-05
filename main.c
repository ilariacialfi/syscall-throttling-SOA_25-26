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

int init_module(void) {
	return dev_register();
}

void cleanup_module(void) {
	dev_unregister();
}

