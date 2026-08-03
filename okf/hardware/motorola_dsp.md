---
type: Hardware Component
title: Motorola DSP
description: Digital signal processor that handles audio mixing, routing, and sample rate conversion.
tags: [dsp, motorola, hardware]
generated: { by: human:reaver, at: 2026-03-08T10:30:00Z }
sources:
  - id: golden-dsp
    resource: ../source/tools/golden/golden_dsp.c
    title: DSP program upload sequence (28388 bytes)
  - id: dsp-bin
    resource: development/firmware/pci_424_dsp.bin
    title: DSP program binary (65536 bytes)
---

# Overview

The MOTU PCI-424 contains a Motorola DSP that performs the real-time
audio processing — mixing, routing, sample rate conversion, and the
multiplexed frame format conversion between the PCI bus and the audio
I/O ports.

# Program Upload

The DSP program is uploaded to [BAR0](/hardware/bars/bar0_dsp.md)
during initialization. The upload consists of thousands of 32-bit
writes to specific offsets in the DSP memory map. The full upload
sequence was captured as the [golden DSP sequence](/re/golden/golden_sequence.md).

# Memory Map

The DSP's address space (accessible via BAR0) includes:

- Program code region
- Coefficient/data tables
- Audio buffer area (DMA buffers)
- DMA descriptor tables (at offsets `0x7008`, `0x70a4`)
- Status/sync registers (at offset `0x70d0`)

# See Also

- [BAR0: DSP Memory](/hardware/bars/bar0_dsp.md)
- [Frame Format](/audio/frame_format.md)
- [Init Sequence Replay](/driver/modules/init_replay.md)
