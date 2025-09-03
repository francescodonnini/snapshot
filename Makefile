obj-m += snapshot.o
snapshot-objs := 	api/activate_snapshot.o \
					api/bio_enqueue.o \
					api/dbg_dump_bio.o \
					api/deactivate_snapshot.o \
					api/hash.o \
					api/loop_utils.o \
					api/registry_rcu.o \
					api/iset_rcu.o \
					api/itree_rcu.o \
					api/session.o \
					api/snapshot.o \
					devices/bnull.o \
					devices/chrdev_ioctl.o \
					devices/chrdev.o \
					probes/handlers.o \
					probes/mount_dev.o \
					probes/submit_bio.o \
					probes/update_session.o \
					main.o \

PWD := $(CURDIR) 

ccflags-y += -I$(src)/include

CFLAGS_api/registry_rcu.o += -DDEBUG
CFLAGS_kretprobes/get_tree_bdev.o += -DDEBUG

all: 
		make -C /lib/modules/$(shell uname -r)/build M=$(PWD)  modules 

mount:
		insmod snapshot.ko

rm:
		rmmod snapshot

clean: 
		make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean