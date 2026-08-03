---
type: Analysis
title: Ghidra Decompilation
description: Decompilation of the Windows driver and application binaries using Ghidra to understand internal logic.
tags: [re, ghidra, decompilation, windows]
generated: { by: human:reaver, at: 2026-03-08T10:30:00Z }
sources:
  - id: ghidra-proj
    resource: ghidra_projects/
    title: Ghidra decompilation projects
  - id: motuaw-asm
    resource: development/disassembly/motuaw.asm
    title: motuaw.sys disassembly (5.5 MB)
  - id: cuemix-asm
    resource: development/disassembly/cuemix.exe.asm
    title: cuemix.exe disassembly (229 MB)
---

# Decompiled Binaries

| Binary                          | Purpose                          | Disassembly Size |
|---------------------------------|----------------------------------|-----------------|
| `motuaw.sys`                    | Windows kernel driver           | 5.5 MB          |
| `cuemix.exe`                    | CueMix FX control application   | 229 MB          |
| `MOTU PCI Audio Console.exe`    | Audio Console application       | 117 MB          |

# Analysis Tools

Over 100 Python scripts were written to analyze the decompiled output:
- `analyze_switch*.py` — Analyze IOCTL dispatch switch statements
- `analyze_ioctl*.py` — Map IOCTL codes to handler functions
- `find_*.py` — Find references, calls, strings, and patterns
- `parse_*.py` — Parse trace output and disassembly

These are in `../source/tools/analysis/`.

# Key Discoveries

- The driver's init sequence is a large block of hardcoded register writes
- DMA buffer addresses are hardcoded physical addresses
- The IOCTL switch statement handles ~30 different control codes
- The CueMix application communicates with the driver via DeviceIoControl

# See Also

- [IOCTL Analysis](/re/ioctl/ioctl_analysis.md)
- [RE Strategy](/re/re_strategy.md)
- [Golden Sequence](/re/golden/golden_sequence.md)
