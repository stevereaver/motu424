# SPDX-License-Identifier: GPL-2.0
#
# Makefile for the MOTU PCI-424 ALSA driver (Linux)
#
# Build:  make -C /lib/modules/$(uname -r)/build M=$(pwd) modules
# Load:   insmod snd_motu424.ko

snd_motu424-objs := motu424_linux_pal.o motu424_alsa.o \
		     ../shared/motu424_fpga.o ../shared/motu424_init.o \
		     ../shared/motu424_dma.o

obj-m += snd_motu424.o

KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean

install: all
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules_install
	depmod -a

install-fw:
	install -d $(DESTDIR)/lib/firmware/motu424/
	install -m 0644 ../shared/init_sequence.bin $(DESTDIR)/lib/firmware/motu424/
	install -m 0644 ../shared/altera424b.rbf $(DESTDIR)/lib/firmware/motu424/

.PHONY: all clean install install-fw
