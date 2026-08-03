---
type: Software Module
title: IRQ Handler
description: Interrupt handler for period-elapsed DMA notifications.
tags: [driver, irq, interrupt, handler]
generated: { by: human:reaver, at: 2026-03-08T10:30:00Z }
sources:
  - id: main-src
    resource: ../source/linux/motu424_alsa.c
    title: motu424_main.c
---

# Overview

The IRQ handler is registered with `IRQF_SHARED` and checks the
[interrupt status register](/registers/interrupt_registers.md) for
the period-elapsed flag.

# Handler Logic

```c
static irqreturn_t motu424_interrupt(int irq, void *dev_id)
{
    struct motu424 *motu = dev_id;
    u32 status;

    status = ioread32(motu->iobase_reg + MOTU_REG_INT_STATUS);
    if (!(status & MOTU_INT_PERIOD_ELAPSED))
        return IRQ_NONE;

    /* Acknowledge interrupt */
    iowrite32(status, motu->iobase_reg + MOTU_REG_INT_STATUS);

    if (motu->playback_substream)
        snd_pcm_period_elapsed(motu->playback_substream);
    if (motu->capture_substream)
        snd_pcm_period_elapsed(motu->capture_substream);

    return IRQ_HANDLED;
}
```

# Interrupt Enable

Interrupts are enabled during probe by writing
`MOTU_INT_PERIOD_ELAPSED` to the interrupt mask register, and
disabled during cleanup by writing `0`.

# See Also

- [Interrupt Registers](/registers/interrupt_registers.md)
- [PCI Probe](/driver/modules/pci_probe.md)
- [PCM Operations](/driver/modules/pcm_operations.md)
