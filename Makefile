obj-m += snapshot.o
snapshot-objs := 	api/activate_snapshot.o \
					api/deactivate_snapshot.o \
					api/find_mount.o \
					api/hash.o \
					api/registry_rcu.o \
					bio/bio_enqueue.o \
					ioctl/chrdev_ioctl.o \
					ioctl/chrdev.o \
					kretprobes/kretprobe_handlers.o \
					kretprobes/mount_bdev.o \
					kretprobes/submit_bio.o \
					exit.o \
					init.o \

PWD := $(CURDIR) 

ccflags-y += -I$(src)/include

CFLAGS_bio/bio_enqueue.o += -DDEBUG
CFLAGS_kretprobes/submit_bio.o += -DDEBUG

all: 
		make -C /lib/modules/$(shell uname -r)/build M=$(PWD)  modules 

mount:
		insmod snapshot.ko

rm:
		rmmod snapshot

clean: 
		make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

test_start:
		insmod snapshot.ko
		sh test/activate.sh /dev/loop0 1234
		make /home/soa/Documents/singlefile-FS/ load-FS-driver
		make /home/soa/Documents/singlefile-FS/ mount-fs

test_end:
		sh test/deactivate.sh /dev/loop0 1234
		rmmod snapshot
		umount /dev/loop0
		rmmod singlefile-FS