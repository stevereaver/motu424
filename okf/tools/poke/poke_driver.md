---
type: Tool
title: Poke Driver
description: Kernel module for directly reading and writing hardware registers on the MOTU PCI-424 for exploration.
tags: [tools, poke, kernel-module, exploration]
generated: { by: human:reaver, at: 2026-03-08T10:30:00Z }
sources:
  - id: poke-driver
    resource: ../source/tools/drivers/motu_poke_driver.c
    title: motu_poke_driver.c
  - id: poke-src
    resource: ../source/tools/poke/
    title: Poke test programs
---

# Overview

The poke driver is a kernel module that provides direct access to the
MOTU PCI-424's BAR registers through a simple interface. It was used
during reverse engineering to test individual register writes and
verify hardware behavior.

# Usage

```bash
# Load the poke driver
insmod motu_poke_driver.ko

# Write to a register
echo "BAR1 0x0000 0x00000007" > /proc/motu_poke

# Read a register
cat /proc/motu_poke
```

# Test Programs

Multiple userspace poke test programs were developed to test specific
hardware behaviors:

| Program           | Purpose                              |
|-------------------|--------------------------------------|
| `poke_test.c`     | Basic register read/write            |
| `poke_fpga.c`     | Test FPGA bitbang loading            |
| `motu_test_bar0.c`| Test BAR0 DSP memory access          |
| `motu_test_regs.c`| Test BAR1 register access            |
| `motu_test_amcc.c`| Test AMCC-specific registers         |

# See Also

- [Replay Tools](/tools/replay/replay_tools.md)
- [Clock Sync Tools](/tools/clock_sync/clock_sync_tools.md)
- [RE Strategy](/re/re_strategy.md)
