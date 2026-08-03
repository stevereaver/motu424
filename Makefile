# SPDX-License-Identifier: GPL-2.0
#
# Top-level Makefile for the MOTU PCI-424 driver project.
#
# Targets:
#   make linux      Build the Linux ALSA kernel module
#   make linux-install  Build + install the Linux module + firmware
#   make windows    Build the Windows WDF driver + test tool (on Windows)
#   make fw         Regenerate init_sequence.bin from golden traces
#   make tools      Build the poke tool (requires kernel headers)
#   make clean      Clean all build outputs
#   make all        Build Linux module + tools
#

.PHONY: all linux linux-install windows fw tools clean help

# Default: build Linux module and tools
all: linux tools

# ── Linux kernel module ──────────────────────────────────────────────────

linux:
	$(MAKE) -C source/linux

linux-install: linux
	$(MAKE) -C source/linux install
	$(MAKE) -C source/linux install-fw

# ── Windows driver (run on Windows with WDK) ────────────────────────────

windows:
	source/windows/build.bat

windows-clean:
	source/windows/build.bat clean

# ── Firmware regeneration ───────────────────────────────────────────────

fw:
	cd source/shared && python3 convert_golden.py \
		../tools/golden/golden_dsp.c \
		../tools/golden/golden_sequence.c \
		init_sequence.bin

# ── Poke tool ───────────────────────────────────────────────────────────

tools:
	$(MAKE) -C source/tools/poke
	$(MAKE) -C source/tools/drivers

# ── Clean ────────────────────────────────────────────────────────────────

clean:
	$(MAKE) -C source/linux clean 2>/dev/null || true
	$(MAKE) -C source/tools/poke clean 2>/dev/null || true
	$(MAKE) -C source/tools/drivers clean 2>/dev/null || true
	@echo "Note: Windows build output in build/windows/ is not cleaned."
	@echo "Run 'source/windows/build.bat clean' on Windows."

# ── Help ────────────────────────────────────────────────────────────────

help:
	@echo "MOTU PCI-424 Driver - Build Targets"
	@echo ""
	@echo "  make linux           Build the Linux ALSA kernel module"
	@echo "  make linux-install   Build + install module + firmware"
	@echo "  make windows         Build the Windows WDF driver (Windows only)"
	@echo "  make fw              Regenerate init_sequence.bin"
	@echo "  make tools           Build poke tool + prototype drivers"
	@echo "  make all             Build Linux module + tools"
	@echo "  make clean           Clean all build outputs"
	@echo "  make help            Show this help"
