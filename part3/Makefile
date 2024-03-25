obj-m += src/elevator.o

KDIR := /lib/modules/$(shell uname -r)/build
CC := gcc-12

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
