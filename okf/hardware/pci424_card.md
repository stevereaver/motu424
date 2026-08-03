---
type: Hardware Device
title: MOTU PCI-424 Audio Interface
description: PCI audio interface card with Altera FPGA and Motorola DSP for multi-channel professional audio I/O.
resource: pci:137a:0004
tags: [motu, pci-424, hardware, pci]
generated: { by: human:reaver, at: 2026-03-08T10:30:00Z }
sources:
  - id: lspci
    resource: development/docs/lspci_output.txt
    title: lspci output for MOTU PCI-424
  - id: qemu-trace
    resource: development/traces/motu_hw_trace.log
    title: QEMU VFIO hardware trace
---

# PCI Identification

| Field        | Value                              |
|--------------|------------------------------------|
| Vendor ID    | `0x137a` (Mark of the Unicorn)    |
| Device ID    | `0x0004`                          |
| PCI Class    | `0x0401` (Multimedia audio controller) |
| Subsystem    | MOTU PCI-424 Audio                |

# BAR Layout

The card exposes three [BAR regions](/hardware/bars/bar0_dsp.md):

| BAR  | Size    | Type           | Purpose                              |
|------|---------|----------------|--------------------------------------|
| BAR0 | 4 MB    | Prefetchable   | [DSP memory](/hardware/bars/bar0_dsp.md) — program and data |
| BAR1 | 8 MB    | Non-prefetchable | [Control registers](/hardware/bars/bar1_registers.md) |
| BAR2 | 16 bytes| I/O port       | [FPGA configuration](/hardware/bars/bar2_fpga_port.md) |

# Initialization Phases

The card requires a multi-phase initialization sequence:

1. **FPGA load** — Bit-bang the Altera bitstream through [BAR2](/hardware/bars/bar2_fpga_port.md)
2. **DSP program upload** — Write the DSP program to [BAR0](/hardware/bars/bar0_dsp.md) memory
3. **Clock/sync configuration** — Configure sample rate and wait for clock lock
4. **DMA setup** — Program DMA buffer addresses and transfer configuration

The full sequence was captured from the Windows driver via [QEMU VFIO tracing](/re/tracing/qemu_vfio.md).

# See Also

- [Altera FPGA](/hardware/altera_fpga.md)
- [Motorola DSP](/hardware/motorola_dsp.md)
- [Driver Overview](/driver/driver_overview.md)
