---
type: Software Module
title: Windows WDF Driver
description: Windows Driver Framework (WDF) driver for the MOTU PCI-424 with custom IOCTL interface for userspace audio access.
tags: [windows, wdf, driver, ioctl]
generated: { by: human:reaver, at: 2026-03-08T11:00:00Z }
sources:
  - id: wdf-src
    resource: ../source/windows/motu424_wdf.c
    title: motu424_wdf.c
  - id: win-pal
    resource: ../source/windows/motu424_win_pal.c
    title: motu424_win_pal.c
  - id: inf
    resource: ../source/windows/motu424.inf
    title: motu424.inf
  - id: original-drv
    resource: original/windows-drv/motuaw.sys
    title: Original Windows driver (motuaw.sys, 400KB)
---

# Overview

The Windows driver is a WDF (Windows Driver Framework) kernel-mode
driver that uses the [shared core](../cross_platform_architecture.md)
via the [PAL](../cross_platform_architecture.md) for all hardware
interaction. It exposes a custom IOCTL interface for userspace audio
applications.

# Driver Entry

`DriverEntry` creates a WDF driver with `EvtDriverDeviceAdd` as the
device add callback. When the PnP manager detects the MOTU PCI-424
(Vendor 0x137a, Device 0x0004), it calls `EvtDriverDeviceAdd`.

# Resource Mapping

`EvtDevicePrepareHardware` parses the PCI translated resources to
find the three BAR regions and maps them with `MmMapIoSpaceEx`. It
then calls the shared core to:
1. Allocate DMA buffer (`motu424_dma_alloc`)
2. Load FPGA bitstream (future — firmware from driver directory)
3. Replay init sequence (future — firmware from driver directory)
4. Poll for clock sync (`motu424_hw_init`)
5. Register interrupt (`pal_irq_request`)

# IOCTL Interface

| IOCTL Code                  | Purpose                          |
|-----------------------------|----------------------------------|
| `IOCTL_MOTU424_GET_INFO`    | Get sample rate, DMA state, port status |
| `IOCTL_MOTU424_START`       | Start DMA playback               |
| `IOCTL_MOTU424_STOP`        | Stop DMA                         |
| `IOCTL_MOTU424_GET_POSITION`| Read DMA position counter       |
| `IOCTL_MOTU424_SET_RATE`    | Set sample rate (future)         |

# Phase 2: PortCls/WaveRT

Future work: implement a PortCls miniport driver
(`motu424_portcls.c`) that registers as a WaveRT audio device,
enabling native Windows audio (WASAPI, DirectSound, MME) without
a userspace bridge.

# Building

Requires WDK (Windows Driver Kit):

```bash
cd ../source/windows/
cmake -B build -S .
cmake --build build
```

# Installation

Install via the INF file using `pnputil` or Device Manager:
```cmd
pnputil /add-driver motu424.inf /install
```

# See Also

- [Cross-Platform Architecture](../cross_platform_architecture.md)
- [PCI-424 Card](../../hardware/pci424_card.md)
- [Original Windows Driver](original_windows_driver.md)
