# SPDX-License-Identifier: GPL-2.0
#
# Makefile for the MOTU PCI-424 poke tool.
#
# Build:  make
# Clean:  make clean
#
# Requires the motu_poke_driver kernel module to be loaded.

CC      ?= gcc
CFLAGS  := -Wall -Wextra -O2 -I../golden
LDLIBS  :=

TARGET  := poke
SRC     := poke.c
GOLDEN  := ../golden/golden_dsp.c ../golden/golden_dsp_patch.c

# The poke.c file #includes the golden DSP sequences directly,
# so we don't compile them separately.

$(TARGET): $(SRC) $(GOLDEN)
	$(CC) $(CFLAGS) -o $@ $(SRC) $(LDLIBS)

clean:
	rm -f $(TARGET)

.PHONY: clean
