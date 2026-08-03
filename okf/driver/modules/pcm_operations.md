---
type: Software Module
title: ALSA PCM Operations
description: Playback and capture PCM operations including open, prepare, trigger, pointer, and ack callbacks.
tags: [driver, alsa, pcm, playback, capture]
generated: { by: human:reaver, at: 2026-03-08T10:30:00Z }
sources:
  - id: pcm-src
    resource: ../source/driver/motu424_pcm.c
    title: motu424_pcm.c
---

# Overview

`motu424_pcm.c` implements the ALSA PCM interface with one playback
and one capture substream, bridging ALSA's stereo format to the card's
[multiplexed frame format](/audio/frame_format.md).

# Hardware Constraints

| Constraint          | Value                              |
|---------------------|------------------------------------|
| Formats             | `S32_LE` (32-bit signed little-endian) |
| Rates               | 44100, 48000, 88200, 96000        |
| Channels            | 2 (stereo)                        |
| Buffer max          | 4 MB                               |
| Period min/max      | 512 bytes / 1 MB                   |
| Periods             | 2–512                              |

# Operations

| Callback    | Playback                          | Capture                           |
|-------------|-----------------------------------|-----------------------------------|
| `open`      | Store substream pointer           | Store substream pointer           |
| `close`     | Clear substream pointer           | Clear substream pointer           |
| `hw_params` | No-op (managed buffer API)        | No-op                             |
| `hw_free`   | No-op                             | No-op                             |
| `prepare`    | Clear DMA buffer, program period  | Set rate                          |
| `trigger`   | Start/stop/pause DMA              | Start/stop/pause DMA              |
| `pointer`   | Read [DMA position](/registers/dma_registers.md) or time-based fallback | Same |
| `ack`       | Expand stereo → multiplexed frame  | Extract stereo ← multiplexed frame |

# Trigger

The [port configuration register](/registers/port_config.md) controls
DMA direction:
- **Playback**: Set `MOTU_PORT_CONF_DMA_RD` (card reads from memory)
- **Capture**: Clear `MOTU_PORT_CONF_DMA_RD` (card writes to memory)

# Ack Callbacks

The `ack` callbacks bridge the ALSA ring buffer and the hardware DMA
buffer by expanding/extracting stereo data into/from the 98-channel
multiplexed frame format. Audio data is at word offsets 2 and 3;
status words 0 and 1 are preserved.

# See Also

- [Frame Format](/audio/frame_format.md)
- [Port Configuration](/registers/port_config.md)
- [DMA Buffer Management](/driver/modules/dma_management.md)
