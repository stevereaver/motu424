---
type: Audio Format
title: Multiplexed Frame Format
description: 98-channel multiplexed audio frame format used by the MOTU PCI-424 for DMA transfers.
tags: [audio, frame-format, dma, multiplexed]
generated: { by: human:reaver, at: 2026-03-08T10:30:00Z }
sources:
  - id: golden-seq
    resource: ../source/tools/golden/golden_sequence.c
    title: Golden register write sequence
---

# Frame Layout

Each sample period, the card transfers a fixed-size frame containing
all channels for all ports via DMA:

```
Frame size = 98 channels × 4 bytes = 392 bytes per frame
```

# Word Layout

| Word Offset | Purpose                              |
|-------------|--------------------------------------|
| 0           | Status/magic word 1 (preserve)      |
| 1           | Status/magic word 2 (preserve)      |
| 2           | Audio channel 0 (left)              |
| 3           | Audio channel 1 (right)             |
| 4–97        | Remaining audio channels            |

# ALSA Bridging

The driver exposes stereo (2-channel) PCM to ALSA. The
[PCM operations](/driver/modules/pcm_operations.md) bridge the
formats:

- **Playback** (`ack` callback): Expand stereo ALSA data into the
  multiplexed frame, writing left/right at word offsets 2 and 3.
- **Capture** (`ack` callback): Extract stereo data from the multiplexed
  frame at the same offsets.

# Limitations

- Only stereo (channels 0–1) is exposed to ALSA. The remaining 96
  channels are zeroed on playback and discarded on capture.
- Multi-channel support would require exposing additional PCM devices
  or using the ALSA "multi" plugin.

# See Also

- [DMA Buffer Management](/driver/modules/dma_management.md)
- [PCM Operations](/driver/modules/pcm_operations.md)
- [Sample Rates](/audio/sample_rates.md)
