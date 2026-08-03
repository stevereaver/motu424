---
type: Software Architecture
title: Cross-Platform Driver Architecture
description: Unified Linux + Windows driver architecture using a shared hardware core and Platform Abstraction Layer.
tags: [architecture, cross-platform, pal, linux, windows]
generated: { by: human:reaver, at: 2026-03-08T11:00:00Z }
sources:
  - id: pal-header
    resource: ../source/shared/motu424_pal.h
    title: PAL interface definition
  - id: source-readme
    resource: ../source/README.md
    title: Source code README
---

# Overview

The MOTU PCI-424 driver uses a **shared core + PAL** architecture that
allows the same hardware interaction code to run on both Linux and
Windows. The hardware logic (FPGA loading, init sequence replay, DMA
setup, register definitions) is identical across platforms — only the
OS interface layer differs.

# Architecture Diagram

```
┌─────────────────────────────────────────────────────┐
│                    ../source/shared/                    │
│   ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌─────────┐ │
│   │ fpga.c   │ │ init.c   │ │ dma.c    │ │ hw.h    │ │
│   │ (PAL)    │ │ (PAL)    │ │ (PAL)    │ │ (defs)  │ │
│   └────┬─────┘ └────┬─────┘ └────┬─────┘ └─────────┘ │
│        └────────────┬┴─────────────┘                  │
│                     │ PAL calls                        │
└─────────────────────┼─────────────────────────────────┘
                      │
         ┌────────────┴────────────┐
         │                         │
┌────────▼────────┐    ┌──────────▼──────────┐
│  ../source/linux/   │    │  ../source/windows/     │
│  ┌─────────────┐ │    │  ┌─────────────────┐ │
│  │ linux_pal.c │ │    │  │ win_pal.c       │ │
│  │ (pci_*)     │ │    │  │ (WDF, MmMapIo)  │ │
│  └──────┬──────┘ │    │  └──────┬──────────┘ │
│         │        │    │         │            │
│  ┌──────▼──────┐ │    │  ┌──────▼────────┐   │
│  │ alsa.c      │ │    │  │ wdf.c         │   │
│  │ (ALSA PCM)  │ │    │  │ (WDF + IOCTL) │   │
│  └─────────────┘ │    │  └───────────────┘  │
└──────────────────┘    └─────────────────────┘
```

# Platform Abstraction Layer (PAL)

The PAL interface (`motu424_pal.h`) defines the contract between the
shared hardware logic and the platform-specific implementations:

| PAL Function           | Linux Implementation          | Windows Implementation           |
|------------------------|-------------------------------|---------------------------------|
| `pal_pci_enable`       | `pci_enable_device()`         | WDF PnP (automatic)             |
| `pal_iomap`            | `pci_iomap()`                 | `MmMapIoSpaceEx()`              |
| `pal_write32`         | `iowrite32()`                 | `WRITE_REGISTER_ULONG()`         |
| `pal_read32`          | `ioread32()`                  | `READ_REGISTER_ULONG()`          |
| `pal_dma_alloc`        | `dma_alloc_coherent()`        | `WdfCommonBufferCreate()`        |
| `pal_irq_request`      | `request_irq()`               | `WdfInterruptCreate()`          |
| `pal_firmware_load`    | `request_firmware()`          | File I/O from driver directory   |
| `pal_msleep`           | `msleep()`                    | `KeDelayExecutionThread()`       |
| `pal_spinlock_*`       | `spinlock_t`                  | `KSPIN_LOCK`                    |
| `pal_log`              | `pr_info/pr_err`              | `DbgPrintEx()`                  |

# Shared Core Modules

| Module           | Purpose                              |
|------------------|--------------------------------------|
| `motu424_fpga.c` | FPGA bitstream loading via bit-bang  |
| `motu424_init.c` | Init sequence replay + DMA translation|
| `motu424_dma.c`  | DMA buffer allocation + SG setup    |
| `motu424_hw.h`   | Hardware definitions, register map  |

# Platform Frontends

## Linux (ALSA)

- `motu424_linux_pal.c` — Implements PAL using Linux kernel APIs
- `motu424_alsa.c` — ALSA card creation, PCM operations, PCI driver

## Windows (WDF)

- `motu424_win_pal.c` — Implements PAL using WDF/WDM APIs
- `motu424_wdf.c` — WDF driver entry, resource mapping, IOCTL interface
- `motu424.inf` — Driver installation INF file

Phase 1 (current): WDF + custom IOCTL for userspace audio bridge
Phase 2 (future): PortCls/WaveRT miniport for native Windows audio

# See Also

- [Driver Overview](driver_overview.md) — Original monolithic driver
- [PCI-424 Card](../hardware/pci424_card.md)
- [RE Strategy](../re/re_strategy.md)
