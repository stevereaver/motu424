# SPDX-License-Identifier: GPL-2.0
#
# Makefile for the MOTU PCI-424 ALSA driver (snd_motu424)
#
# Build:  make -C /lib/modules/$(uname -r)/build M=$(pwd) modules
# Load:   insmod snd_motu424.ko
#
# Required firmware files (install in /lib/firmware/motu424/):
#   altera424b.rbf       - Altera FPGA bitstream
#   init_sequence.bin     - Golden register-write init sequence

snd_motu424-objs := motu424_main.o motu424_fpga.o motu424_init.o \
		     motu424_dma.o motu424_pcm.o

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

# Install firmware files to /lib/firmware/motu424/
install-fw:
	install -d $(DESTDIR)/lib/firmware/motu424/
	install -m 0644 init_sequence.bin $(DESTDIR)/lib/firmware/motu424/
	install -m 0644 altera424b.rbf $(DESTDIR)/lib/firmware/motu424/

.PHONY: all clean install install-fw
