---
type: Audio Configuration
title: Sample Rates
description: Supported sample rates and clock configuration for the MOTU PCI-424.
tags: [audio, sample-rate, clock]
generated: { by: human:reaver, at: 2026-03-08T10:30:00Z }
sources:
  - id: clock-sync
    resource: ../source/tools/clock_sync/motu_clock_sync.c
    title: Clock sync tool
---

# Supported Rates

| Rate     | Status     |
|---------|------------|
| 44100   | Supported  |
| 48000   | Tested     |
| 88200   | Supported  |
| 96000   | Supported  |

The driver currently defaults to 48000 Hz and only 48 kHz has been
tested with the [init sequence](/driver/modules/init_replay.md).

# Clock Configuration

The sample rate is configured during the [init sequence replay](/driver/modules/init_replay.md).
Changing the sample rate requires re-running the init sequence with
different clock configuration values.

# Clock Sync

After initialization, the driver polls the [sync status registers](/registers/sync_status.md)
to verify the card has achieved clock lock. The card may need up to 2
seconds to achieve lock depending on attached I/O boxes.

# ALSA Hardware Constraints

The [PCM operations](/driver/modules/pcm_operations.md) expose these
rates to ALSA via `snd_pcm_hardware`:

```c
.rates = (SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_48000 |
          SNDRV_PCM_RATE_88200 | SNDRV_PCM_RATE_96000),
.rate_min = 44100,
.rate_max = 96000,
```

# See Also

- [Sync Status Registers](/registers/sync_status.md)
- [Clock Sync Tools](/tools/clock_sync/clock_sync_tools.md)
- [PCM Operations](/driver/modules/pcm_operations.md)
