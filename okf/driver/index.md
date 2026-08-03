---
type: Index
title: Driver Architecture Concepts
description: Documentation of the clean-room ALSA kernel driver modules.
tags: [driver]
generated: { by: human:reaver, at: 2026-03-08T10:30:00Z }
---

# Driver Architecture

- [Cross-Platform Architecture](cross_platform_architecture.md) — Unified Linux + Windows with PAL
- [Driver Overview](driver_overview.md) — Module structure and init flow

## Modules

- [PCI Probe](modules/pci_probe.md) — Device enumeration and BAR mapping
- [FPGA Loading](modules/fpga_loading.md) — Bit-banged bitstream upload
- [Init Sequence Replay](modules/init_replay.md) — Golden sequence with DMA translation
- [DMA Buffer Management](modules/dma_management.md) — Coherent buffer allocation
- [PCM Operations](modules/pcm_operations.md) — ALSA playback/capture interface
- [IRQ Handler](modules/irq_handler.md) — Interrupt handling
- [Windows WDF Driver](modules/windows_wdf.md) — Windows WDF + IOCTL interface
- [Windows Build and Test](modules/windows_build.md) — How to build, install, and test on Windows
- [Original Windows Driver](modules/original_windows_driver.md) — motuaw.sys analysis
- [24I/O Init Failure Analysis](modules/24io_init_failure.md) — Root cause and fixes for 24I/O init
