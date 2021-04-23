obj-m := temp.o
KERNEL_DIR := /home/Hi3559AV100_SDK_V2.0.1.0/osdrv/opensource/kernel/linux-4.9.y_multi-core/
PWD := $(shell pwd)
all:
	make -C $(KERNEL_DIR) SUBDIRS=$(PWD) modules ARCH=arm64 CROSS_COMPILE=aarch64-himix100-linux-
clean:
	rm *.o *.ko *.mod.c

.PHONY:clean
