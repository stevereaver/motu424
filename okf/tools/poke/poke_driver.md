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
    resource: ../source/tools/poke/poke.c
    title: Unified poke tool
  - id: poke-archive
    resource: ../source/tools/poke/archive/
    title: Archived poke test programs
---

# Overview

The poke driver is a kernel module that provides direct access to the
MOTU PCI-424's BAR registers through a simple interface. It was used
during reverse engineering to test individual register writes and
verify hardware behavior.

# Usage

```bash
# Load the poke driver
cd source/tools/drivers
make
sudo insmod motu_poke_driver.ko

# Build the poke tool
cd ../poke
make

# Read a register
./poke read 1 0x00

# Write a register
./poke write 1 0x00 0x000F2782

# Scan a BAR for non-zero values
./poke scan 1 0x0 0x100000

# Monitor a register
./poke monitor 1 0x1C 50

# Load FPGA firmware + DSP program
./poke fpga ../../../firmware/altera424b.rbf
```

# Unified Poke Tool

The `poke.c` tool consolidates all earlier poke test programs into a
single tool with subcommands: `read`, `write`, `scan`, `monitor`, and
`fpga`. See `source/tools/poke/README.md` for full documentation.

# Archived Programs

Earlier iterative versions are in `source/tools/poke/archive/`:

| Program            | Purpose                              |
|--------------------|--------------------------------------|
| `poke_test.c`      | Basic register read/write (no bar)   |
| `poke_test3.c`     | Added bar support                    |
| `poke_test10.c`    | Clock/rate fuzzing                    |
| `poke_test11.c`    | BAR1 mixer RAM scan                   |
| `poke_fpga.c`      | FPGA bitbang + DSP load (standalone)  |
| `motu_poke.c`     | mmap-based BAR1 monitor               |
| `motu_test_bar0.c`| mmap-based BAR0 DSP memory test       |
| `motu_test_regs.c`| mmap-based BAR1 register test          |
| `motu_test_amcc.c`| mmap-based AMCC register test         |

# See Also

- [Replay Tools](/tools/replay/replay_tools.md)
- [Clock Sync Tools](/tools/clock_sync/clock_sync_tools.md)
- [RE Strategy](/re/re_strategy.md)
