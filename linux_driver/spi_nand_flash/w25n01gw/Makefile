obj-m := w25n01gw.o
KERNEL_DIR := /home/wangzh/work/Hi3519AV100_SDK_V2.0.1.0/osdrv/opensource/kernel/linux-4.9.y-smp
PWD := $(shell pwd)
all:
	make -C $(KERNEL_DIR) SUBDIRS=$(PWD) modules ARCH=arm CROSS_COMPILE=arm-himix200-linux-
clean:
	rm *.o *.ko *.mod.c

.PHONY:clean
