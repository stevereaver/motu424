---
type: Update Log
title: MOTU PCI-424 Knowledge Base — Change History
description: Chronological history of updates to this OKF bundle.
tags: [changelog]
generated: { by: human:reaver, at: 2026-03-08T10:30:00Z }
---

# Update History

## 2026-03-08

- **Created** initial OKF bundle from reverse-engineering work on MOTU PCI-424.
- **Documented** hardware identification (Vendor 0x137a, Device 0x0004) and BAR layout.
- **Captured** golden sequence of 7192 register writes via QEMU VFIO tracing.
- **Reverse-engineered** FPGA bitbang protocol for Altera bitstream loading.
- **Built** clean-room ALSA driver (`../source/driver/`) with FPGA load, init replay, DMA, and PCM.
- **Analyzed** Windows driver IOCTL handlers via Ghidra decompilation.
- **Developed** poke drivers and clock sync tools for hardware exploration.
- **Refactored** driver into cross-platform architecture: `../source/shared/` (PAL + hardware core), `../source/linux/` (ALSA), `../source/windows/` (WDF).
- **Fixed** 24I/O initialization failure: corrected FPGA bitbang protocol, added DSP RAM zeroing, DSP kick-start, and post-init port configuration loop.
- **Reorganized** all development artifacts from `/linux` into `/development` with subdirectories: traces, disassembly, binaries, docs, firmware, test, build, misc.
- **Updated** all OKF document resource paths to reflect new `/development` locations.
- **Built** Windows 11 WDF driver: installed WDK 26100, fixed MSVC compatibility (packed structs, format attributes, kernel-safe logging), created build script, compiled `motu424.sys` (17 KB) and `motu424_test.exe` (148 KB).
- **Created** Windows test suite: `motu424_test.c` (userspace IOCTL tool with 12 built-in tests), `test.ps1` (PowerShell test runner with 16 tests), `install.bat`/`install.ps1` (driver installation scripts).
- **Documented** Windows build process in OKF.
