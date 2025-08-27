obj-m += snapshot.o
snapshot-objs := 	api/activate_snapshot.o \
					api/deactivate_snapshot.o \
					api/hash.o \
					api/loop_utils.o \
					api/path_utils.o \
					api/registry_rcu.o \
					api/hashset_rcu.o \
					api/session.o \
					api/snapshot.o \
					bio/bio_enqueue.o \
					bio/dbg_dump_bio.o \
					bnull/bnull.o \
					ioctl/chrdev_ioctl.o \
					ioctl/chrdev.o \
					kretprobes/kretprobe_handlers.o \
					kretprobes/mount_bdev.o \
					kretprobes/ext4_fill_super.o \
					kretprobes/get_tree.o \
					kretprobes/path_umount.o \
					kretprobes/submit_bio.o \
					kretprobes/update_session.o \
					main.o \

PWD := $(CURDIR) 

ccflags-y += -I$(src)/include

CFLAGS_api/snapshot.o += -DDEBUG
CFLAGS_bio/dbg_dump_bio.o += -DDEBUG
CFLAGS_kretprobes/*.o += -DDEBUG


all: 
		make -C /lib/modules/$(shell uname -r)/build M=$(PWD)  modules 

mount:
		insmod snapshot.ko

rm:
		rmmod snapshot

clean: 
		make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean