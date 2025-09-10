obj-m += snapshot.o
snapshot-objs := 	api/activate_snapshot.o \
					api/dbg_dump_bio.o \
					api/deactivate_snapshot.o \
					api/hash.o \
					api/loop_utils.o \
					api/registry_rcu.o \
					api/itree_rcu.o \
					api/session.o \
					api/snap_map.o \
					api/snapshot.o \
					devices/bnull.o \
					devices/chrdev_ioctl.o \
					devices/chrdev.o \
					rbitmap/array16.o \
					rbitmap/bitset16.o \
					rbitmap/rbitmap32.o \
					probes/handlers.o \
					probes/ext4_fill_super.o \
					probes/get_tree_bdev.o \
					probes/mount_bdev.o \
					probes/path_umount.o \
					probes/submit_bio.o \
					probes/update_session.o \
					main.o \

PWD := $(CURDIR) 

ccflags-y += -I$(src)/include
ccflags-y += -I$(src)/rbitmap

CFLAGS_api/dbg_dump_bio.o += -DDEBUG
CFLAGS_api/registry_rcu.o += -DDEBUG
CFLAGS_api/session.o += -DDEBUG
CFLAGS_kretprobes/get_tree_bdev.o += -DDEBUG
CFLAGS_kretprobes/mount_dev.o += -DDEBUG
CFLAGS_kretprobes/submit_bio.o += -DDEBUG
CFLAGS_probes/handlers.o += -DDEBUG

all: 
		make -C /lib/modules/$(shell uname -r)/build M=$(PWD)  modules 

mount:
		insmod snapshot.ko

rm:
		rmmod snapshot

clean: 
		make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean