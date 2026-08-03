---
type: Hardware Region
title: BAR2 — FPGA Configuration Port (16 bytes I/O)
description: 16-byte I/O port used for FPGA bitstream loading via bit-banged serial protocol.
resource: pci:137a:0004:bar2
tags: [bar2, fpga, io-port]
generated: { by: human:reaver, at: 2026-03-08T10:30:00Z }
sources:
  - id: bitbang-trace
    resource: ../source/tools/golden/golden_fpga_bitbang.c
    title: Captured FPGA bitbang sequence
  - id: fpga-fw
    resource: development/firmware/pci_424_fw.bin
    title: Altera FPGA bitstream firmware
---

# Overview

BAR2 is a 16-byte I/O port that provides the FPGA configuration
interface. It is used to load the [Altera FPGA](/hardware/altera_fpga.md)
bitstream during card initialization.

# Port Layout

| Offset | Purpose                          |
|--------|----------------------------------|
| `0x00` | FPGA status (read)              |
| `0x04` | FPGA configuration data (write) |

# Status Register

The status byte at offset `0x00` indicates FPGA state:

| Bits | Meaning                          |
|------|----------------------------------|
| `0x13`| FPGA configured + clock locked   |
| Other | FPGA not ready or clock not locked|

# Loading Sequence

The FPGA is loaded by bit-banging the bitstream through BAR1 offset
`0x300008`, not directly through BAR2. BAR2 is used to read status
before and after loading. See [FPGA Bitbang Protocol](/re/fpga/bitbang_protocol.md)
for the full protocol.

# See Also

- [Altera FPGA](/hardware/altera_fpga.md)
- [FPGA Loading](/driver/modules/fpga_loading.md)
