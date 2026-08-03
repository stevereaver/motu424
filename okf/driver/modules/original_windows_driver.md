---
type: Reference
title: Original Windows Driver (motuaw.sys)
description: Analysis of the original MOTU Windows kernel driver used as the basis for reverse engineering.
tags: [windows, original, motuaw, reverse-engineering]
generated: { by: human:reaver, at: 2026-03-08T11:00:00Z }
sources:
  - id: motuaw-sys
    resource: original/windows-drv/motuaw.sys
    title: motuaw.sys (400080 bytes)
  - id: motuaw-inf
    resource: original/windows-drv/cab_extracted2/MOTUAW.inf
    title: MOTUAW.inf
  - id: motuaw-asm
    resource: original/windows-drv/motuaw.sys.asm
    title: motuaw.sys disassembly (14.9 MB)
  - id: ghidra-proj
    resource: ghidra_projects/motu-windows-drv/
    title: Ghidra decompilation project
---

# Overview

The original Windows driver (`motuaw.sys`, 400 KB) is a WDM kernel-mode
driver that provides audio functionality for the MOTU PCI-424 card.
It was the primary source for reverse engineering the hardware
initialization sequence.

# INF Analysis

The `MOTUAW.inf` file identifies:
- PCI Vendor `0x137a`, Devices `0x0003`, `0x0004`, `0x0005`
- Driver class: Media (`{4D36E96C-E325-11CE-BFC1-08002BE10318}`)
- Service name: `MotuAW`
- Start type: `SERVICE_DEMAND_START` (3)
- The driver also loads a child wave driver (`mawwave64`)

# Driver Architecture

The original driver is a WDM (Windows Driver Model) driver that:
1. Handles PnP device detection for PCI VEN_137A&DEV_0004
2. Maps BARs and initializes the FPGA + DSP
3. Exposes audio through a wave port driver (`MAWWAVE.sys`)
4. Handles IOCTLs from `CueMix FX.exe` and `MOTU PCI Audio Console.exe`
5. Provides ASIO support via `MAWASIO.DRV` (360 KB)

# Related Files

| File              | Purpose                              |
|-------------------|--------------------------------------|
| `motuaw.sys`      | Main kernel driver (400 KB)         |
| `MAWWAVE.sys`     | Wave port driver (86 KB)            |
| `MAWASIO.DRV`     | ASIO driver (360 KB)                |
| `MotuBus64.sys`   | Bus driver for child devices (43 KB)|
| `MOTUFWA64.sys`   | FireWire audio driver (662 KB)      |
| `MOTUPCIVideo.sys`| PCI video driver (100 KB)           |

# Decompilation

The driver was decompiled using Ghidra. Key findings:
- Large block of hardcoded register writes for initialization
- IOCTL switch statement with ~30 control codes
- Hardcoded physical addresses for DMA buffers
- Uses `WRITE_REGISTER_ULONG` for MMIO access

See [Ghidra Decompilation](../../re/ghidra_decompilation.md) and
[IOCTL Analysis](../../re/ioctl/ioctl_analysis.md) for details.

# See Also

- [Windows WDF Driver](windows_wdf.md) — Our replacement driver
- [Cross-Platform Architecture](../cross_platform_architecture.md)
- [Golden Sequence](../../re/golden/golden_sequence.md)
