obj-m += throttler.o
throttler-objs += main.o device.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
clean:
	make -C /lib/modules/$(shell uname -r)build M=$(PWD) clean
