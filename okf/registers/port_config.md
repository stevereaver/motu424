---
type: Register
title: Port Configuration Register (BAR1 + 0x0000)
description: Controls DMA enable, run state, transfer direction, and stream start.
resource: pci:137a:0004:bar1:0x0000
tags: [register, port-config, dma]
generated: { by: human:reaver, at: 2026-03-08T10:30:00Z }
sources:
  - id: golden-seq
    resource: ../source/tools/golden/golden_sequence.c
    title: Golden register write sequence
---

# Register Details

| Field                    | Bit | Description                              |
|--------------------------|-----|------------------------------------------|
| `MOTU_PORT_CONF_DMA_EN`  | 0   | Enable DMA transfers                    |
| `MOTU_PORT_CONF_RUN`     | 1   | Run the audio engine                    |
| `MOTU_PORT_CONF_DMA_RD`  | 2   | DMA direction: 1=read (playback), 0=write (capture) |
| `MOTU_PORT_CONF_START`   | 3   | Start stream                            |

# Usage

## Playback Start

```c
val = ioread32(bar1 + MOTU_REG_PORT_CONF);
val |= MOTU_PORT_CONF_DMA_EN | MOTU_PORT_CONF_RUN | MOTU_PORT_CONF_DMA_RD;
iowrite32(val, bar1 + MOTU_REG_PORT_CONF);
```

## Capture Start

```c
val = ioread32(bar1 + MOTU_REG_PORT_CONF);
val |= MOTU_PORT_CONF_DMA_EN | MOTU_PORT_CONF_RUN;
val &= ~MOTU_PORT_CONF_DMA_RD;
iowrite32(val, bar1 + MOTU_REG_PORT_CONF);
```

## Stop

```c
val &= ~(MOTU_PORT_CONF_DMA_EN | MOTU_PORT_CONF_RUN |
         MOTU_PORT_CONF_DMA_RD | MOTU_PORT_CONF_START);
iowrite32(val, bar1 + MOTU_REG_PORT_CONF);
```

# See Also

- [Control Registers](/registers/control_registers.md)
- [PCM Operations](/driver/modules/pcm_operations.md)
