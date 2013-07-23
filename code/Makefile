ifneq ($(KERNELRELEASE),)
	obj-m := seqfile.o
else
	KERNELDIR ?= /lib/modules/$(shell uname -r)/build
	PWD = $(shell pwd)
default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
endif

clean:
	$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(shell pwd) clean
