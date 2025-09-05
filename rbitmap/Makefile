obj-m += rbitmap_test.o
rbitmap_test-objs := 	array16.o \
					 	bitset16.o \
					 	rbitmap32.o \
					 	rbitmap32_test.o \

PWD := $(CURDIR) 

all: 
		make -C /lib/modules/$(shell uname -r)/build M=$(PWD)  modules 

load:
		insmod rbitmap_test.ko

rm:
		rmmod rbitmap_test

clean: 
		make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean