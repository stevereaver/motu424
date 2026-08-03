---
type: Methodology
title: Reverse Engineering Strategy
description: Overall approach to reverse-engineering the MOTU PCI-424 Windows driver and building a Linux replacement.
tags: [re, strategy, methodology]
generated: { by: human:reaver, at: 2026-03-08T10:30:00Z }
sources:
  - id: re-strategy
    resource: development/docs/motu_re_strategy.md
    title: RE strategy document
  - id: dev-strategy
    resource: development/docs/driver_development_strategy.md
    title: Driver development strategy
---

# Approach

The reverse engineering strategy has three phases:

1. **Trace capture** — Use QEMU/KVM with VFIO PCI passthrough to run
   the original Windows driver and capture all PCI BAR accesses.
   See [QEMU VFIO Tracing](/re/tracing/qemu_vfio.md).

2. **Static analysis** — Decompile the Windows driver binaries
   (`motuaw.sys`, `cuemix.exe`) using Ghidra to understand the
   driver's internal logic, IOCTL handlers, and data structures.
   See [Ghidra Decompilation](/re/ghidra_decompilation.md).

3. **Replay and build** — Replay the captured register writes on
   real hardware using [poke drivers](/tools/poke/poke_driver.md)
   and [replay tools](/tools/replay/replay_tools.md), then build
   a clean-room ALSA driver from the observed behavior.

# Key Findings

- The card requires a multi-phase init: FPGA load → DSP upload → clock sync
- The init sequence is ~7192 register writes, captured as the
  [golden sequence](/re/golden/golden_sequence.md)
- DMA addresses are hardcoded in the Windows driver and must be
  translated at replay time
- Audio uses a [multiplexed 98-channel frame format](/audio/frame_format.md)

# See Also

- [Driver Overview](/driver/driver_overview.md)
- [QEMU VFIO Tracing](/re/tracing/qemu_vfio.md)
- [Ghidra Decompilation](/re/ghidra_decompilation.md)
