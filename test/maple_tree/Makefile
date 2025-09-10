obj-m += maple_tree.o
maple_tree-objs := main.o

PWD := $(CURDIR) 

CFLAGS_main.o += -DDEBUG

all: 
		make -C /lib/modules/$(shell uname -r)/build M=$(PWD)  modules 

mount:
		insmod maple_tree.ko

rm:
		rmmod maple_tree

clean: 
		make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean