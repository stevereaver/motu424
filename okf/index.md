---
type: Knowledge Bundle
title: MOTU PCI-424 Driver Knowledge Base
description: Reverse-engineered hardware documentation and driver architecture for the MOTU PCI-424 audio interface.
tags: [motu, pci-424, audio, driver, alsa, reverse-engineering]
generated: { by: human:reaver, at: 2026-03-08T10:30:00Z }
---

# MOTU PCI-424 Driver Knowledge Base

This OKF bundle documents the hardware, reverse-engineering methodology,
and driver architecture for the Mark of the Unicorn (MOTU) PCI-424 audio
interface — a PCI card with an Altera FPGA and Motorola DSP that provides
multi-channel professional audio I/O.

## Concepts

### Hardware

- [PCI-424 Card](hardware/pci424_card.md) — PCI vendor/device IDs, BAR layout, hardware identification
- [BAR0: DSP Memory](hardware/bars/bar0_dsp.md) — 4 MB prefetchable memory region for DSP program and data
- [BAR1: Control Registers](hardware/bars/bar1_registers.md) — 8 MB non-prefetchable register space
- [BAR2: FPGA Config Port](hardware/bars/bar2_fpga_port.md) — 16-byte I/O port for FPGA configuration
- [Altera FPGA](hardware/altera_fpga.md) — FPGA bitstream loading and configuration
- [Motorola DSP](hardware/motorola_dsp.md) — DSP program upload and execution model

### Register Map

- [Control Registers](registers/control_registers.md) — BAR1 register offsets and bitfields
- [Port Configuration](registers/port_config.md) — DMA direction, run/enable, start bits
- [DMA Registers](registers/dma_registers.md) — DMA base address, size, control
- [Interrupt Registers](registers/interrupt_registers.md) — Interrupt mask, status, acknowledge
- [Sync Status Registers](registers/sync_status.md) — Clock lock and sync polling

### Audio

- [Frame Format](audio/frame_format.md) — Multiplexed 98-channel frame layout
- [Sample Rates](audio/sample_rates.md) — Supported sample rates and clock configuration

### Driver Architecture

- [Cross-Platform Architecture](driver/cross_platform_architecture.md) — Unified Linux + Windows driver with PAL
- [Driver Overview](driver/driver_overview.md) — Module structure and initialization flow
- [PCI Probe](driver/modules/pci_probe.md) — Device enumeration and BAR mapping
- [FPGA Loading](driver/modules/fpga_loading.md) — Bit-banged bitstream upload
- [Init Sequence Replay](driver/modules/init_replay.md) — Golden sequence replay with DMA translation
- [DMA Buffer Management](driver/modules/dma_management.md) — Coherent buffer allocation and SG setup
- [PCM Operations](driver/modules/pcm_operations.md) — ALSA playback/capture interface
- [IRQ Handler](driver/modules/irq_handler.md) — Interrupt handling and period elapsed
- [Windows WDF Driver](driver/modules/windows_wdf.md) — Windows WDF driver with IOCTL interface
- [Original Windows Driver](driver/modules/original_windows_driver.md) — Analysis of motuaw.sys

### Reverse Engineering

- [RE Strategy](re/re_strategy.md) — Overall reverse-engineering approach
- [QEMU VFIO Tracing](re/tracing/qemu_vfio.md) — PCI passthrough trace capture methodology
- [Golden Sequence](re/golden/golden_sequence.md) — 7192 captured register writes
- [FPGA Bitbang Protocol](re/fpga/bitbang_protocol.md) — Reverse-engineered FPGA loading sequence
- [IOCTL Analysis](re/ioctl/ioctl_analysis.md) — Windows driver IOCTL handler analysis
- [Ghidra Decompilation](re/ghidra_decompilation.md) — Decompilation of Windows binaries

### Tools

- [Poke Driver](tools/poke/poke_driver.md) — Kernel module for hardware register exploration
- [Replay Tools](tools/replay/replay_tools.md) — Trace replay with DMA address translation
- [Clock Sync Tools](tools/clock_sync/clock_sync_tools.md) — Clock synchronization and sync-finder experiments

### Development Artifacts

- [Development Artifacts](development/development_artifacts.md) — Logs, traces, disassembly, binaries, and firmware from the RE process
