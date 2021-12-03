#KERNEL_SRC =/home/zgt/work/imx6ull/buildroot-2017.02.5/output/build/linux-custom
KERNEL_SRC =/home/uisrc/work/2G4chn/linux-custom
SUBDIR = $(shell pwd) 

#CC=arm-linux-gcc 
#LD=arm-linux-ld
all: modules
obj-m := lacheck.o
modules:
	make -C $(KERNEL_SRC) M=$(SUBDIR) modules
	rm *.o *mod.c *.symvers *.order 
	rm .fpga_i2c* -f
	rm .tmp_versions -rf
clean:
	-rm *.ko
