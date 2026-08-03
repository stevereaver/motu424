---
type: Software Module
title: FPGA Bitstream Loading
description: Bit-banged serial loading of the Altera FPGA bitstream through BAR1.
tags: [driver, fpga, bitbang, loading]
generated: { by: human:reaver, at: 2026-03-08T10:30:00Z }
sources:
  - id: fpga-src
    resource: ../source/shared/motu424_fpga.c
    title: motu424_fpga.c
  - id: bitbang-trace
    resource: ../source/tools/golden/golden_fpga_bitbang.c
    title: Captured FPGA bitbang sequence
---

# Overview

`motu424_fpga.c` loads the Altera FPGA bitstream (`altera424b.rbf`,
37302 bytes) by bit-banging serial data through [BAR1 offset 0x300008](/hardware/bars/bar1_registers.md).

# Loading Sequence

1. **Pre-configure** — Write initialization values to BAR1 registers
2. **Bit-bang loop** — For each byte of the bitstream:
   - Write the byte to BAR1 offset `0x300008`
   - Toggle clock/control bits
   - Call `cond_resched()` every 16384 bytes to avoid hogging CPU
3. **Status check** — Read [BAR2 status](/hardware/bars/bar2_fpga_port.md)
   and verify it reads `0x13` (configured + locked)

# Firmware

The bitstream is loaded via `request_firmware()` with the name
`motu424/altera424b.rbf`. If the firmware is missing, the driver
assumes a warm boot (FPGA already loaded from a previous session).

# See Also

- [Altera FPGA](/hardware/altera_fpga.md)
- [BAR2: FPGA Config Port](/hardware/bars/bar2_fpga_port.md)
- [FPGA Bitbang Protocol](/re/fpga/bitbang_protocol.md)
