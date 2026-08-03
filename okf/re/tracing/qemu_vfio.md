---
type: Methodology
title: QEMU VFIO PCI Tracing
description: Methodology for capturing PCI BAR accesses from the Windows driver using QEMU/KVM with VFIO passthrough.
tags: [re, qemu, vfio, tracing]
generated: { by: human:reaver, at: 2026-03-08T10:30:00Z }
sources:
  - id: trace-log
    resource: development/traces/motu_hw_trace.log
    title: Raw QEMU VFIO trace (59 MB)
  - id: trace-log2
    resource: development/traces/motu_hw_trace2.log
    title: Second QEMU VFIO trace (61 MB)
  - id: qemu-events
    resource: development/misc/qemu-trace-events
    title: QEMU trace event configuration
---

# Setup

1. **Host**: Linux machine with the MOTU PCI-424 card installed
2. **Hypervisor**: QEMU/KVM with VFIO PCI passthrough
3. **Guest**: Windows with the official MOTU driver installed
4. **Tracing**: QEMU trace events enabled for the MOTU device's
   BAR regions

# QEMU Configuration

The VM was started with the MOTU card passed through via VFIO:

```bash
qemu-system-x86_64 \
  -enable-kvm \
  -device vfio-pci,host=06:01.0 \
  -trace events=qemu-trace-events \
  ...
```

# Trace Events

The `qemu-trace-events` file enables tracing of PCI MMIO and I/O
port accesses to the MOTU device's BAR regions.

# Captured Data

The trace produces a log of all `ioread32`/`iowrite32` operations
performed by the Windows driver, including:
- BAR0 (DSP memory) writes — the DSP program upload
- BAR1 (control registers) writes — configuration and DMA setup
- BAR2 (FPGA port) accesses — FPGA bitbang sequence

The raw trace (60 MB) was processed into the
[golden sequence](/re/golden/golden_sequence.md) for replay.

# See Also

- [Golden Sequence](/re/golden/golden_sequence.md)
- [FPGA Bitbang Protocol](/re/fpga/bitbang_protocol.md)
- [RE Strategy](/re/re_strategy.md)
