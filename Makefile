obj-m += snapshot.o
snapshot-objs := activate_snapshot.o chrdev_ioctl.o chrdev.o deactivate_snapshot.o exit.o hash.o init.o registry.o

PWD := $(CURDIR) 

CFLAGS_activate_snapshot.o += -DDEBUG
CFLAGS_chrdev.o += -DDEBUG
CFLAGS_chrdev_ioctl.o += -DDEBUG
CFLAGS_deactivate_snapshot.o += -DDEBUG
CFLAGS_hash.o += -DDEBUG
CFLAGS_init.o += -DDEBUG
CFLAGS_registry.o += -DDEBUG

all: 
		make -C /lib/modules/$(shell uname -r)/build M=$(PWD)  modules 

mount:
		insmod snapshot.ko

rm:
		rmmod snapshot

clean: 
		make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
