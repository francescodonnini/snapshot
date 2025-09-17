obj-m += snapshot.o
snapshot-objs := 	core/activate_snapshot.o \
					core/auth.o \
					core/dbg_dump_bio.o \
					core/deactivate_snapshot.o \
					core/hash.o \
					core/loop_utils.o \
					core/registry_rcu.o \
					core/itree_rcu.o \
					core/session.o \
					core/snap_map.o \
					core/snapshot.o \
					devices/bnull.o \
					devices/chrdev_ioctl.o \
					devices/chrdev.o \
					rbitmap/array16.o \
					rbitmap/bitset16.o \
					rbitmap/rbitmap32.o \
					probes/handlers.o \
					probes/fill_super.o \
					probes/submit_bio.o \
					probes/update_session.o \
					main.o \

PWD := $(CURDIR) 

ccflags-y += -I$(src)/include
ccflags-y += -I$(src)/rbitmap

CFLAGS_core/dbg_dump_bio.o += -DDEBUG
CFLAGS_core/registry_rcu.o += -DDEBUG
CFLAGS_core/session.o += -DDEBUG
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