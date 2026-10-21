obj-m += throttler.o
throttler-objs += main.o device.o registry.o sctable.o lib/scth.o

A := $(shell cat /sys/module/the_usctm/parameters/sys_call_table_address 2>/dev/null)

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
mount:
	@rmmod throttler 2>/dev/null || true
	@test -n "$(A)" || { echo "ERROR: load the discoverer first (sudo insmod the_usctm.ko)"; exit 1; }
	insmod throttler.ko the_syscall_table=$(A)
	@rm -f /dev/throttler
	mknod /dev/throttler c $$(grep throttler /proc/devices | cut -d' ' -f1) 0
unmount:
	rm -f /dev/throttler
	rmmod throttler

