---
type: Register Map
title: Control Registers (BAR1)
description: Memory-mapped control registers accessible via BAR1 for DMA, port configuration, interrupts, and clock.
tags: [registers, bar1, control]
generated: { by: human:reaver, at: 2026-03-08T10:30:00Z }
sources:
  - id: golden-seq
    resource: ../source/tools/golden/golden_sequence.c
    title: Golden register write sequence
  - id: smart-replay
    resource: ../source/tools/replay/smart_replay.c
    title: Smart replay tool
---

# Register Summary

All registers are 32-bit, accessed via `ioread32`/`iowrite32` on
[BAR1](/hardware/bars/bar1_registers.md).

| Offset     | Name                | Access | Description                     |
|------------|---------------------|--------|---------------------------------|
| `0x0000`   | Port Configuration | R/W    | [Port config](/registers/port_config.md) |
| `0x0004`   | DMA Control         | R/W    | [DMA control/position](/registers/dma_registers.md) |
| `0x0008`   | DMA Base Address    | R/W    | Physical DMA buffer address     |
| `0x000c`   | DMA Size            | R/W    | Transfer size                   |
| `0x0010`   | Interrupt Mask      | R/W    | [IRQ enable bits](/registers/interrupt_registers.md) |
| `0x0014`   | Interrupt Status    | R      | Pending IRQ flags               |
| `0x300008` | FPGA Bitbang Data   | W      | Serial data for FPGA loading    |

# Access Patterns

The Windows driver writes to these registers in a specific sequence
during initialization. The [golden sequence](/re/golden/golden_sequence.md)
captures the full write trace. During audio streaming, the driver
reads the DMA control register for position and writes the port
configuration register for start/stop.

# See Also

- [Port Configuration](/registers/port_config.md)
- [DMA Registers](/registers/dma_registers.md)
- [Interrupt Registers](/registers/interrupt_registers.md)
