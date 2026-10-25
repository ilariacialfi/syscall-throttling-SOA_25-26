obj-m += throttler.o
throttler-objs += main.o device.o registry.o sctable.o lib/scth.o

A := $(shell cat /sys/module/the_usctm/parameters/sys_call_table_address 2>/dev/null)

all: module userspace

module:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
userspace:
	gcc user/throttlectl.c -o user/throttlectl
	gcc test/test_syscall_by_nr.c -o test/test_syscall_by_nr
	gcc test/test_args.c -o test/test_args
	gcc test/test_blockingsys.c -o test/test_blockingsys
	gcc test/test_blocked.c -o test/test_blocked
	gcc test/test_ioctl.c -o test/test_ioctl
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm -f user/throttlectl test/test_syscall_by_nr test/test_args test/test_blockingsys test/test_blocked test/test_ioctl
mount:
	@rmmod throttler 2>/dev/null || true
	@test -n "$(A)" || { echo "ERROR: load the discoverer first (sudo insmod the_usctm.ko)"; exit 1; }
	insmod throttler.ko the_syscall_table=$(A)
	@rm -f /dev/throttler
	mknod /dev/throttler c $$(grep throttler /proc/devices | cut -d' ' -f1) 0
unmount:
	rm -f /dev/throttler
	rmmod throttler

