#
# Makefile by giann
#
ARCH	:=arm 
COMPILER	:=/opt/FriendlyARM/toolschain/4.5.1/bin/arm-linux-
#KERNELDIR	:=/home/giann/smart210/LinuxResource/linux-3.0.8/normal_build
KERNELDIR 	:=/path/to/kernel
PWD := $(shell pwd)

obj-m += btn_led_drv.o
all:
	make -C $(KERNELDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(COMPILER) modules
	arm-linux-gcc -o app_test app_test.c

clean:
	make -C $(KERNELDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(COMPILER) clean
	rm -f app_test
