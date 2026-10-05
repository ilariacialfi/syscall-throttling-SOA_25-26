obj-m += throttler.o
throttler-objs += main.o device.o registry.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
clean:
	make -C /lib/modules/$(shell uname -r)build M=$(PWD) clean
mount:
	-rmmod throttler
	insmod throttler.ko
	rm -f /dev/throttler
	mknod /dev/throttler c $$(grep throttler /proc/devices | cut -d' ' -f1) 0

unmount:
	rm -f /dev/throttler
	rmmod throttler

