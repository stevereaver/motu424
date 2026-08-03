---
type: Analysis
title: 24I/O Initialization Failure Analysis
description: Root cause analysis of why the 24I/O audio interface box failed to initialize on Linux, and the fixes applied.
tags: [bug, 24io, fpga, dsp, init, cold-boot]
generated: { by: human:reaver, at: 2026-03-08T12:00:00Z }
sources:
  - id: poke-fpga
    resource: ../source/tools/poke/poke_fpga.c
    title: Working FPGA + DSP init tool
  - id: bitbang-trace
    resource: development/traces/bitbang_head.txt
    title: Captured FPGA bitbang values
  - id: post-init-trace
    resource: development/traces/post_init.txt
    title: Post-init port configuration trace
  - id: cold-boot-notes
    resource: development/traces/fpgu_cold_boot.txt
    title: Cold boot strategy notes
---

# Overview

The 24I/O audio interface box connected to the PCI-424 card was not
initializing on Linux. Five root causes were identified and fixed.

# Root Causes

## 1. FPGA bitbang protocol was completely wrong (CRITICAL)

The shared `motu424_fpga.c` was writing **whole bytes** to the data
register instead of **individual bits** — it wasn't actually
bit-banging at all. It also used the wrong register for the clock
(`0x30000c` instead of bit 7 of `0x300008`) and wrong control values
(`0x01/0x00` instead of `0xE0/0xE0`).

**Correct protocol** (from `poke_fpga.c` and `bitbang_head.txt`):

| Value | Meaning              |
|-------|----------------------|
| 0x40  | Clock low, data = 0  |
| 0x60  | Clock low, data = 1  |
| 0xC0  | Clock high, data = 0 |
| 0xE0  | Clock high, data = 1 |

Each bit requires 3 writes to `0x300008`: data setup, data hold,
clock pulse (OR 0x80). The protocol also requires a BAR2 reset
(`0x4 = 0x1`) before and finalization (`0x300004 = 0xC0`,
`BAR2 0x8 = 0x0`) after.

**Status**: Fixed in `../source/shared/motu424_fpga.c`.

## 2. Missing DSP RAM zeroing

The working `poke_fpga.c` tool zeros 64K words of DSP RAM (BAR0)
before loading the program. The driver was skipping this step,
leaving stale DSP state from previous sessions.

**Status**: Fixed — `motu424_zero_dsp_ram()` added to init sequence.

## 3. Missing DSP kick-start (CRITICAL)

After loading the DSP program, the DSP must be started with:
```
BAR2 0x0   = 0x0   (reset DSP)
BAR0 0x3fffc = 0x0 (clear boot vector)
BAR2 0x4   = 0x2   (start DSP running)
wait 500ms
```

Without this, the DSP sits idle and never configures the audio
ports. The 24I/O never sees clock/sync because the DSP never runs.

**Status**: Fixed — `motu424_dsp_kickstart()` added to init sequence.

## 4. Missing post-init port configuration loop

The `post_init.txt` trace shows a repeating loop that configures 4
audio ports after the golden sequence:

```
Read  BAR0 0x6fd8  (DMA address)
Write BAR0 0x7040 = 0x0
Read  BAR0 0x6fec
Write BAR0 0x6fec = 0x0
Read  BAR2 0x0     (status check, expect 0x13)
Write BAR1 0x400000 = 0x10 (port strobe)
```

This loop cycles through 4 DMA addresses (ports) and was not in the
golden sequence or the driver. The 24I/O has 4 audio ports that need
this configuration.

**Status**: Fixed — `motu424_post_init_port_loop()` added.

## 5. No interface detection (FUTURE)

The Windows driver has separate source files for different interface
boxes (`AW24IO.cpp`, `AW2408.cpp`). Our driver doesn't detect which
interface is connected and uses a generic init sequence.

**Status**: Not yet fixed — requires reverse engineering `AW24IO.cpp`
to identify 24I/O-specific register writes.

# Fixes Applied

A new `motu424_full_init()` function chains all initialization steps:

1. **FPGA bitstream load** (cold boot) — correct bit-bang protocol
2. **DSP RAM zeroing** — clear 64K words of BAR0
3. **Golden sequence replay** — DSP program + patches + register config
4. **DSP kick-start** — reset, clear boot vector, start, wait
5. **Post-init port loop** — 32 iterations of 4-port configuration
6. **Sync polling** — wait for clock lock (0x13 status)

Both the old `../source/linux/` and new `../source/shared/` + `../source/linux/`
code paths now call `motu424_full_init()`.

# See Also

- [FPGA Loading](fpga_loading.md)
- [Init Sequence Replay](init_replay.md)
- [FPGA Bitbang Protocol](../../re/fpga/bitbang_protocol.md)
- [Cross-Platform Architecture](../cross_platform_architecture.md)
