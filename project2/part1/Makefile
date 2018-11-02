SDK_HOME = /opt/iot-devkit/1.7.2/sysroots

PATH := $(PATH):$(SDK_HOME)/x86_64-pokysdk-linux/usr/bin/i586-poky-linux

ARCH = x86
CROSS_COMPILE = i586-poky-linux-
SROOT=$(SDK_HOME)/i586-poky-linux/

ifeq ($(CC),i586-poky-linux-gcc)

all:
	$(CC) -Wall -o program.o program.c --sysroot=$(SROOT) -lm
else

all:
	gcc -Wall -o program.o program.c -lm
endif

clean:
	rm -f *.o
