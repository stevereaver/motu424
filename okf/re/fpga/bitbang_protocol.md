---
type: Protocol
title: FPGA Bitbang Protocol
description: Reverse-engineered serial bit-bang protocol for loading the Altera FPGA bitstream through BAR1.
tags: [re, fpga, bitbang, protocol]
generated: { by: human:reaver, at: 2026-03-08T10:30:00Z }
sources:
  - id: bitbang-trace
    resource: ../source/tools/golden/golden_fpga_bitbang.c
    title: Captured FPGA bitbang sequence (34 MB)
  - id: fpga-fw
    resource: development/firmware/pci_424_fw.bin
    title: Altera FPGA bitstream (37302 bytes)
  - id: poke-fpga
    resource: ../source/tools/poke/poke_fpga.c
    title: FPGA poke tool
---

# Overview

The Altera FPGA bitstream is loaded by bit-banging serial data through
[BAR1 offset 0x300008](/hardware/bars/bar1_registers.md). The protocol
was reverse-engineered from the QEMU VFIO trace.

# Protocol

1. **Pre-configuration** — Write initialization values to BAR1 registers
   to put the FPGA into configuration mode:
   - Write `0x01` to BAR1 + `0x300000`
   - Write `0x00` to BAR1 + `0x300004`

2. **Bit-bang loop** — For each byte of the bitstream:
   ```c
   iowrite32(byte, bar1 + 0x300008);
   iowrite32(0x01, bar1 + 0x30000c);  /* clock high */
   iowrite32(0x00, bar1 + 0x30000c);  /* clock low */
   ```
   Call `cond_resched()` every 16384 bytes.

3. **Post-configuration** — Write finalization values to BAR1 registers

4. **Status check** — Read [BAR2 + 0x00](/hardware/bars/bar2_fpga_port.md)
   and verify it reads `0x13` (configured + locked)

# Bitstream

The bitstream is in Altera Raw Binary File (`.rbf`) format — a raw
sequence of configuration bytes. The firmware file
(`altera424b.rbf`, 37302 bytes) is loaded via `request_firmware()`.

# Captured Trace

The full bitbang sequence was captured as `golden_fpga_bitbang.c`
(34 MB), containing every individual BAR1 write. This was used to
derive the protocol above.

# See Also

- [Altera FPGA](/hardware/altera_fpga.md)
- [FPGA Loading](/driver/modules/fpga_loading.md)
- [BAR2: FPGA Config Port](/hardware/bars/bar2_fpga_port.md)
