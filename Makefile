obj-m += snapshot.o
snapshot-objs := 	api/activate_snapshot.o \
					api/deactivate_snapshot.o \
					api/find_mount.o \
					api/hash.o \
					api/registry_rcu.o \
					ioctl/chrdev_ioctl.o \
					ioctl/chrdev.o \
					kretprobes/init.o \
					kretprobes/mount_bdev.o \
					kretprobes/submit_bio.o \
					exit.o \
					init.o \

PWD := $(CURDIR) 

ccflags-y += -I$(src)/include

CFLAGS_kretprobes/init.o += -DDEBUG
CFLAGS_kretprobes/mount_bdev.o += -DDEBUG
CFLAGS_kretprobes/submit_bio.o += -DDEBUG

all: 
		make -C /lib/modules/$(shell uname -r)/build M=$(PWD)  modules 

mount:
		insmod snapshot.ko

rm:
		rmmod snapshot

clean: 
		make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
