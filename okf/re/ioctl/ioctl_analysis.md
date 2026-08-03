---
type: Analysis
title: IOCTL Handler Analysis
description: Analysis of the Windows driver's IOCTL handlers for mixer control, clock source, and audio routing.
tags: [re, ioctl, windows, ghidra]
generated: { by: human:reaver, at: 2026-03-08T10:30:00Z }
sources:
  - id: ioctl-asm
    resource: development/disassembly/ioctl_switch.asm
    title: IOCTL switch disassembly
  - id: deviceio
    resource: development/traces/DeviceIOControl.txt
    title: DeviceIOControl trace
---

# Overview

The Windows driver (`motuaw.sys`) exposes control through DeviceIoControl
IOCTLs. These were analyzed via Ghidra decompilation to understand
mixer controls, clock source selection, and audio routing.

# IOCTL Codes

The driver uses a standard IRP_MJ_DEVICE_CONTROL dispatch with a
switch statement on IOCTL codes. The switch was disassembled and
analyzed using multiple [Python analysis scripts](/tools/poke/poke_driver.md)
(`analyze_switch*.py`, `analyze_ioctl*.py`).

# Key Findings

- **Mixer controls** — Volume, mute, and phantom power are controlled
  via specific IOCTL codes that write to BAR0 DSP memory
- **Clock source** — Internal vs. external clock selection via IOCTL
  writes to BAR1 registers
- **Audio routing** — Channel routing matrices configured through
  DSP memory writes

# Current Status

The Linux driver does not yet implement mixer controls. Mapping these
IOCTL handlers to ALSA control elements is a future task.

# See Also

- [Ghidra Decompilation](/re/ghidra_decompilation.md)
- [RE Strategy](/re/re_strategy.md)
- [Driver Overview](/driver/driver_overview.md)
