---
type: Reference Data
title: Golden Sequence
description: 7192 captured register writes from the QEMU VFIO trace of the Windows driver, used as the initialization sequence.
tags: [re, golden, trace, init-sequence]
generated: { by: human:reaver, at: 2026-03-08T10:30:00Z }
sources:
  - id: golden-seq
    resource: ../source/tools/golden/golden_sequence.c
    title: Golden register write sequence (263676 bytes)
  - id: golden-dsp
    resource: ../source/tools/golden/golden_dsp.c
    title: DSP program upload sequence (28388 bytes)
  - id: golden-patch
    resource: ../source/tools/golden/golden_dsp_patch.c
    title: Patched DSP sequence (258617 bytes)
  - id: trace-log
    resource: development/traces/motu_hw_trace.log
    title: Raw QEMU VFIO trace
---

# Overview

The golden sequence is a complete capture of all register writes
performed by the Windows driver during card initialization. It
contains 7192 writes across BAR0, BAR1, and BAR2.

# Components

| File                      | Description                              |
|---------------------------|------------------------------------------|
| `golden_sequence.c`       | Main register write sequence (6491 writes) |
| `golden_dsp.c`            | DSP program upload (701 writes to BAR0) |
| `golden_fpga_bitbang.c`   | FPGA bitbang sequence (34 MB, bit-level) |
| `golden_dsp_patch.c`      | Patched DSP sequence with DMA addresses  |

# Binary Format

The golden sequence is converted to a compact binary blob
(`init_sequence.bin`, 64 KB) for runtime loading via
`request_firmware()`. Each entry is 9 bytes:

```
[bar: 1 byte] [offset: 4 bytes LE] [value: 4 bytes LE]
```

# DMA Address Translation

The Windows driver hardcoded physical addresses for its DMA buffers.
The [replay engine](/driver/modules/init_replay.md) translates these
to our own DMA buffer address at runtime. See
[smart_replay.c](/tools/replay/replay_tools.md) for the reference
translation implementation.

# Regeneration

```bash
cd ../source/linux/
python3 convert_golden.py \
    ../tools/golden/golden_dsp.c \
    ../tools/golden/golden_sequence.c \
    init_sequence.bin
```

# See Also

- [Init Sequence Replay](/driver/modules/init_replay.md)
- [QEMU VFIO Tracing](/re/tracing/qemu_vfio.md)
- [Replay Tools](/tools/replay/replay_tools.md)
