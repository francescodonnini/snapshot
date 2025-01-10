obj-m += snapshot.o
snapshot-objs := init.o exit.o hash.o

PWD := $(CURDIR) 

CFLAGS_init.o += -DDEBUG
CFLAGS_exit.o += -DDEBUG
CFLAGS_hash.o += -DDEBUG

all: 
		make -C /lib/modules/$(shell uname -r)/build M=$(PWD)  modules 

mount:
		insmod snapshot.ko

clean: 
		make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
