---
type: Hardware Region
title: BAR0 — DSP Memory (4 MB Prefetchable)
description: Prefetchable memory-mapped region containing the DSP program and audio data buffers.
resource: pci:137a:0004:bar0
tags: [bar0, dsp, memory]
generated: { by: human:reaver, at: 2026-03-08T10:30:00Z }
sources:
  - id: golden-seq
    resource: ../source/tools/golden/golden_sequence.c
    title: Golden register write sequence
  - id: golden-dsp
    resource: ../source/tools/golden/golden_dsp.c
    title: DSP program upload sequence
---

# Overview

BAR0 is a 4 MB prefetchable memory region that serves as the DSP's
address space. It contains:

- **DSP program code** — Uploaded during initialization
- **Audio data buffers** — DMA transfer buffers for playback/capture
- **DMA descriptor tables** — Scatter-gather descriptors at fixed offsets
- **Status registers** — Sync and clock lock status

# Key Offsets

| Offset    | Purpose                              |
|-----------|--------------------------------------|
| `0x7008`  | DMA base address (playback)         |
| `0x70a4`  | DMA base address (capture)           |
| `0x70a8`  | Buffer configuration                |
| `0x70ac`  | Transfer configuration              |
| `0x70d0`  | Sync status register                |

# DMA Descriptor Format

The card uses a simple (base_address, size) pair at fixed offsets.
For a single contiguous buffer, one entry is written. See
[DMA Buffer Management](/driver/modules/dma_management.md) for details.

# Initialization

During the [init sequence replay](/driver/modules/init_replay.md), the
DSP program is uploaded by writing thousands of 32-bit values to BAR0.
The [golden DSP sequence](/re/golden/golden_sequence.md) contains
the full upload trace.

# See Also

- [BAR1: Control Registers](/hardware/bars/bar1_registers.md)
- [Motorola DSP](/hardware/motorola_dsp.md)
- [DMA Registers](/registers/dma_registers.md)
