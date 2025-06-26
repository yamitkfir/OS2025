obj-m := message_slot.o
KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules
	gcc -O3 -Wall -std=c11 message_sender.c -o message_sender
	gcc -O3 -Wall -std=c11 message_reader.c -o message_reader
 
clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
