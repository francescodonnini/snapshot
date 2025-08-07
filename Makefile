obj-m += snapshot.o
snapshot-objs := 	api/activate_snapshot.o \
					api/deactivate_snapshot.o \
					api/hash.o \
					api/loop_utils.o \
					api/path_utils.o \
					api/registry_rcu.o \
					api/snapset_rcu.o \
					api/snapshot.o \
					bio/bio_enqueue.o \
					bio/dbg_dump_bio.o \
					bnull/bnull.o \
					ioctl/chrdev_ioctl.o \
					ioctl/chrdev.o \
					kretprobes/kretprobe_handlers.o \
					kretprobes/mount_bdev.o \
					kretprobes/submit_bio.o \
					exit.o \
					init.o \

PWD := $(CURDIR) 

ccflags-y += -I$(src)/include

CFLAGS_api/registry_rcu.o += -DDEBUG
CFLAGS_bio/bio_enqueue.o += -DDEBUG
CFLAGS_bnull/bnull.o += -DDEBUG
CFLAGS_ioctl/chrdev_ioctl.o += -DDEBUG
CFLAGS_ioctl/chrdev.o += -DDEBUG
CFLAGS_kretprobes/kretprobe_handlers.o += -DDEBUG
CFLAGS_kretprobes/submit_bio.o += -DDEBUG
CFLAGS_bio/dbg_dump_bio.o += -DDEBUG
CFLAGS_kretprobes/mount_bdev.o += -DDEBUG
CFLAGS_init.o += -DDEBUG
CFLAGS_api/snapshot.o += -DDEBUG
CFLAGS_api/path_utils.o += -DDEBUG

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

test_end:
		sh test/deactivate.sh /dev/loop0 1234
		rmmod snapshot