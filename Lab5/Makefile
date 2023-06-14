
obj-m     += kshram.o
ccflags-y += -DEXPORT_SYMTAB

all: hello
	make ARCH=x86_64 CROSS_COMPILE=x86_64-linux-gnu- -C /home/alison/Lab5/dist/dist/modulebuild M=$(PWD) modules

hello: hello.c
	x86_64-linux-gnu-gcc -o $@ $< -Wall -static

clean:
	rm -f hello
	make -C /home/alison/Lab5/dist/dist/modulebuild M=$(PWD) clean

install: all
	mkdir -p /home/alison/Lab5/dist/dist/rootfs/modules
	cp hello hellomod.ko /home/alison/Lab5/dist/dist/rootfs/modules
