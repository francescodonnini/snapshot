obj-m += snapshot.o
snapshot-objs := 	api/activate_snapshot.o \
					api/bio_enqueue.o \
					api/dbg_dump_bio.o \
					api/deactivate_snapshot.o \
					api/hash.o \
					api/loop_utils.o \
					api/registry_rcu.o \
					api/hashset_rcu.o \
					api/session.o \
					api/snapshot.o \
					devices/bnull.o \
					devices/chrdev_ioctl.o \
					devices/chrdev.o \
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

all: 
		make -C /lib/modules/$(shell uname -r)/build M=$(PWD)  modules 

mount:
		insmod snapshot.ko

rm:
		rmmod snapshot

clean: 
		make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean