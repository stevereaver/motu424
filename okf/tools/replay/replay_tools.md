---
type: Tool
title: Replay Tools
description: Tools for replaying captured register-write traces on real hardware with DMA address translation.
tags: [tools, replay, trace, dma-translation]
generated: { by: human:reaver, at: 2026-03-08T10:30:00Z }
sources:
  - id: smart-replay
    resource: ../source/tools/replay/smart_replay.c
    title: smart_replay.c
  - id: replay-trace
    resource: ../source/tools/replay/replay_trace.c
    title: replay_trace.c
  - id: fix-bitbang
    resource: ../source/tools/replay/fix_bitbang.c
    title: fix_bitbang.c
---

# Overview

The replay tools take the [golden sequence](/re/golden/golden_sequence.md)
captured from QEMU VFIO tracing and replay it on real hardware,
translating the Windows driver's hardcoded DMA addresses to the
Linux driver's own buffer.

# smart_replay.c

The primary replay tool. It:
1. Loads the golden sequence
2. Allocates a DMA buffer
3. For each register write, translates DMA addresses if needed
4. Writes the value to the appropriate BAR

# DMA Address Translation

The translation logic identifies Windows hardcoded addresses and
replaces them with our DMA buffer address:

```c
if ((val & 0xFFFFF000) == 0x10914000)
    return dma_addr + (val & 0xFFF);
if (val == 0xFE870000)
    return dma_addr;
if (val == 0x90000000)
    return dma_addr;
if ((val & 0xFFFF0000) == 0xBFD70000)
    return dma_addr + (val & 0xFFFF);
if ((val & 0xFFFFF000) == 0xBFF92000)
    return dma_addr + 0x222000 + (val & 0xFFF);
```

This logic is now integrated into the driver's
[init sequence replay](/driver/modules/init_replay.md) engine.

# See Also

- [Golden Sequence](/re/golden/golden_sequence.md)
- [Init Sequence Replay](/driver/modules/init_replay.md)
- [DMA Buffer Management](/driver/modules/dma_management.md)
