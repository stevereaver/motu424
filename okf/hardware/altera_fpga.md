---
type: Hardware Component
title: Altera FPGA
description: Field-programmable gate array that implements the PCI-to-DSP bridge logic and audio routing.
tags: [fpga, altera, hardware]
generated: { by: human:reaver, at: 2026-03-08T10:30:00Z }
sources:
  - id: fpga-fw
    resource: development/firmware/pci_424_fw.bin
    title: Altera FPGA bitstream (37302 bytes)
  - id: bitbang-trace
    resource: ../source/tools/golden/golden_fpga_bitbang.c
    title: Captured FPGA bitbang sequence
---

# Overview

The MOTU PCI-424 contains an Altera FPGA that acts as the bridge between
the PCI bus and the Motorola DSP. The FPGA must be loaded with a bitstream
(`altera424b.rbf`, 37302 bytes) before the card can function.

# Bitstream Format

The bitstream is in Altera Raw Binary File (`.rbf`) format — a raw
sequence of configuration bytes that are shifted into the FPGA one bit
at a time via a bit-bang protocol.

# Loading Process

1. Pre-configure BAR1 registers to put the FPGA into configuration mode
2. Bit-bang each byte of the bitstream through [BAR1 offset 0x300008](/hardware/bars/bar1_registers.md)
3. Read [BAR2 status](/hardware/bars/bar2_fpga_port.md) to verify configuration
4. Check that status reads `0x13` (configured + locked)

See [FPGA Bitbang Protocol](/re/fpga/bitbang_protocol.md) for the
reverse-engineered protocol details and [FPGA Loading](/driver/modules/fpga_loading.md)
for the driver implementation.

# See Also

- [BAR2: FPGA Config Port](/hardware/bars/bar2_fpga_port.md)
- [Motorola DSP](/hardware/motorola_dsp.md)
