---
type: Software Module
title: DMA Buffer Management
description: Coherent DMA buffer allocation and scatter-gather descriptor setup for audio transfers.
tags: [driver, dma, buffer, allocation]
generated: { by: human:reaver, at: 2026-03-08T10:30:00Z }
sources:
  - id: dma-src
    resource: ../source/shared/motu424_dma.c
    title: motu424_dma.c
---

# Overview

`motu424_dma.c` allocates a single large coherent DMA buffer (up to 4 MB)
and sets up the scatter-gather descriptor table in [DSP memory](/hardware/bars/bar0_dsp.md).

# Allocation

```c
motu->dma_buf = dma_alloc_coherent(&pci->dev, size, &motu->dma_addr, GFP_KERNEL);
```

The buffer is zeroed after allocation. The physical address is stored
for use by the [init sequence replay](/driver/modules/init_replay.md)
and [PCM operations](/driver/modules/pcm_operations.md).

# Descriptor Setup

The DMA descriptor is written to BAR0 DSP memory:

| Offset    | Value                              |
|----------|-------------------------------------|
| `0x7008` | DMA base address (playback)        |
| `0x70a4` | DMA base address (capture)          |
| `0x70a8` | Buffer config (`0x30000`)          |
| `0x70ac` | Transfer config (`0x59c`)          |

The DMA base address is also written to [BAR1 register 0x0008](/registers/dma_registers.md).

# Buffer Limits

| Parameter                  | Value      |
|---------------------------|------------|
| `MOTU_DMA_BUF_MAX`        | 4 MB       |
| `MOTU_DMA_PERIOD_BYTES_MIN` | 512 bytes |
| `MOTU_DMA_PERIOD_BYTES_MAX` | 1 MB     |
| `MOTU_DMA_PERIODS_MIN`    | 2          |
| `MOTU_DMA_PERIODS_MAX`    | 512        |

# See Also

- [DMA Registers](/registers/dma_registers.md)
- [Frame Format](/audio/frame_format.md)
- [PCM Operations](/driver/modules/pcm_operations.md)
