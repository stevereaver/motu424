---
type: Register
title: Interrupt Registers (BAR1 + 0x0010–0x0014)
description: Interrupt mask and status registers for period-elapsed notifications.
resource: pci:137a:0004:bar1:0x0010
tags: [register, interrupt, irq]
generated: { by: human:reaver, at: 2026-03-08T10:30:00Z }
---

# Register Details

| Offset   | Name              | Access | Description                          |
|----------|-------------------|--------|--------------------------------------|
| `0x0010` | Interrupt Mask    | R/W    | Enable/disable interrupt sources    |
| `0x0014` | Interrupt Status  | R      | Pending interrupt flags              |

# Interrupt Sources

| Flag                      | Bit | Description                          |
|---------------------------|-----|--------------------------------------|
| `MOTU_INT_PERIOD_ELAPSED` | 0   | A DMA period has been transferred   |

# Usage

## Enable Interrupts

```c
iowrite32(MOTU_INT_PERIOD_ELAPSED, bar1 + MOTU_REG_INT_MASK);
```

## Disable Interrupts

```c
iowrite32(0, bar1 + MOTU_REG_INT_MASK);
```

# Handler

The [IRQ handler](/driver/modules/irq_handler.md) reads the status
register, checks for the period-elapsed flag, and calls
`snd_pcm_period_elapsed()` for the active playback/capture substreams.

# See Also

- [Control Registers](/registers/control_registers.md)
- [IRQ Handler](/driver/modules/irq_handler.md)
