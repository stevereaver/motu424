---
type: Hardware Region
title: BAR1 — Control Registers (8 MB Non-Prefetchable)
description: Non-prefetchable memory-mapped register space for card control, DMA configuration, and interrupt management.
resource: pci:137a:0004:bar1
tags: [bar1, registers, control]
generated: { by: human:reaver, at: 2026-03-08T10:30:00Z }
sources:
  - id: golden-seq
    resource: ../source/tools/golden/golden_sequence.c
    title: Golden register write sequence
  - id: smart-replay
    resource: ../source/tools/replay/smart_replay.c
    title: Smart replay tool with DMA translation
---

# Overview

BAR1 is an 8 MB non-prefetchable memory region containing the card's
control registers. All register accesses for DMA configuration, port
control, interrupt management, and clock setup go through this region.

# Key Register Offsets

| Offset     | Register              | Purpose                          |
|------------|----------------------|----------------------------------|
| `0x0000`   | Port Configuration   | DMA enable, run, direction       |
| `0x0004`   | DMA Control          | DMA position counter             |
| `0x0008`   | DMA Base Address     | Physical address of DMA buffer  |
| `0x000c`   | DMA Size             | Transfer size in bytes           |
| `0x0010`   | Interrupt Mask       | IRQ enable/disable               |
| `0x0014`   | Interrupt Status     | Pending interrupt flags          |
| `0x300008` | FPGA Bitbang Data    | Serial data for FPGA loading     |

# FPGA Bitbang

The [FPGA bitstream](/hardware/altera_fpga.md) is loaded by writing
serial data to offset `0x300008` in a specific bit-bang protocol. See
[FPGA Bitbang Protocol](/re/fpga/bitbang_protocol.md) for details.

# See Also

- [Control Registers](/registers/control_registers.md)
- [Port Configuration](/registers/port_config.md)
- [DMA Registers](/registers/dma_registers.md)
- [Interrupt Registers](/registers/interrupt_registers.md)
