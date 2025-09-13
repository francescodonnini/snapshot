obj-m += rbitmap.o
rbitmap-objs := main.o \
			   ../../rbitmap/array16.o \
			   ../../rbitmap/bitset16.o \
			   ../../rbitmap/rbitmap32.o \

PWD := $(CURDIR) 

CFLAGS_main.o += -DDEBUG

all: 
		make -C /lib/modules/$(shell uname -r)/build M=$(PWD)  modules 

mount:
		insmod rbitmap.ko

rm:
		rmmod rbitmap

clean: 
		make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean