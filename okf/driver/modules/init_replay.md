---
type: Software Module
title: Init Sequence Replay Engine
description: Replays the golden register-write sequence with DMA address translation and sync polling.
tags: [driver, init, replay, dma-translation]
generated: { by: human:reaver, at: 2026-03-08T10:30:00Z }
sources:
  - id: init-src
    resource: ../source/driver/motu424_init.c
    title: motu424_init.c
  - id: smart-replay
    resource: ../source/tools/replay/smart_replay.c
    title: Smart replay tool (reference implementation)
  - id: golden-seq
    resource: ../source/tools/golden/golden_sequence.c
    title: Golden register write sequence
---

# Overview

`motu424_init.c` replays a captured sequence of 7192 register writes
to initialize the card's DSP program, clock configuration, and audio
routing. The sequence is stored as a compact binary blob
(`init_sequence.bin`, 64 KB) loaded via `request_firmware()`.

# Binary Blob Format

Each entry is 9 bytes:

```
[bar: 1 byte] [offset: 4 bytes LE] [value: 4 bytes LE]
```

# DMA Address Translation

The Windows driver hardcoded several physical addresses for its DMA
buffers. The replay engine translates these to our own DMA buffer
address at runtime:

| Windows Address    | Translation                          |
|--------------------|--------------------------------------|
| `0x10914xxx`       | Our DMA base (page-aligned)         |
| `0xFE870000`       | Our DMA base                         |
| `0x90000000`       | Our DMA base                         |
| `0xBFD70xxx`       | Our DMA base + offset                |
| `0xBFF92xxx`       | Our DMA base + 0x222000 + offset     |

If no DMA buffer is allocated, translation is skipped (warm boot path).

# Sync Polling

After replay, `motu424_hw_init()` polls:
1. [BAR0 sync status](/registers/sync_status.md) for `0x0` (not busy)
2. [BAR2 port status](/registers/bars/bar2_fpga_port.md) for `0x13` (locked)

Timeout is 2 seconds. Failures are logged as warnings but do not
prevent driver loading.

# Regeneration

```bash
cd ../source/driver/
python3 convert_golden.py ../tools/golden/golden_dsp.c ../tools/golden/golden_sequence.c init_sequence.bin
```

# See Also

- [Golden Sequence](/re/golden/golden_sequence.md)
- [DMA Buffer Management](/driver/modules/dma_management.md)
- [Sync Status Registers](/registers/sync_status.md)
