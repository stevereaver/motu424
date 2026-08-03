---
type: Register
title: DMA Registers (BAR1 + 0x0004–0x000c)
description: DMA base address, size, and position counter registers for audio stream transfers.
resource: pci:137a:0004:bar1:0x0004
tags: [register, dma, transfer]
generated: { by: human:reaver, at: 2026-03-08T10:30:00Z }
sources:
  - id: golden-seq
    resource: ../source/tools/golden/golden_sequence.c
    title: Golden register write sequence
  - id: smart-replay
    resource: ../source/tools/replay/smart_replay.c
    title: Smart replay tool with DMA translation
---

# Register Details

| Offset   | Name              | Description                              |
|----------|-------------------|------------------------------------------|
| `0x0004` | DMA Control       | Read: current DMA position (byte offset) |
| `0x0008` | DMA Base Address  | Physical address of DMA buffer           |
| `0x000c` | DMA Size          | Transfer size in bytes                   |

# DMA Position Counter

The DMA control register (`0x0004`) returns the current byte position
of the DMA engine within the buffer. The driver uses this for the ALSA
[pointer callback](/driver/modules/pcm_operations.md).

If the position counter is not valid (returns 0 or a value larger than
the buffer), the driver falls back to a time-based estimate using
`ktime_get_ns()` and the sample rate.

# DMA Address Translation

The Windows driver hardcoded physical addresses for its DMA buffers.
The [init sequence replay](/driver/modules/init_replay.md) translates
these to our own DMA buffer address. See
[DMA Buffer Management](/driver/modules/dma_management.md) for the
allocation and setup.

# BAR0 DMA Descriptors

In addition to the BAR1 registers, DMA descriptors are written to
[BAR0 DSP memory](/hardware/bars/bar0_dsp.md) at offsets `0x7008`
and `0x70a4`.

# See Also

- [Control Registers](/registers/control_registers.md)
- [Port Configuration](/registers/port_config.md)
- [DMA Buffer Management](/driver/modules/dma_management.md)
