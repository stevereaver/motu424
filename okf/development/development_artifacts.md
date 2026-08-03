---
type: Reference
title: Development Artifacts
description: Logs, traces, disassembly, binaries, and firmware blobs generated during the reverse-engineering and driver development process.
tags: [development, traces, disassembly, binaries, firmware]
generated: { by: human:reaver, at: 2026-03-08T12:30:00Z }
---

# Overview

The `/development` directory contains all artifacts generated during
the reverse-engineering and driver development process. These files
were originally in `/linux` and have been reorganized into a structured
layout for clarity.

# Directory Structure

```
development/
├── traces/         QEMU VFIO traces, dmesg logs, and captured I/O
├── disassembly/    Ghidra/objdump disassembly of Windows binaries
├── binaries/       Compiled RE tools and kernel modules
├── docs/           Strategy documents, prompts, and lspci output
├── firmware/       Extracted firmware blobs (FPGA, DSP)
├── test/           Test audio files (WAV)
├── build/          Kernel build artifacts (.cmd files, Module.symvers)
└── misc/           Miscellaneous files (qemu-trace-events, etc.)
```

## traces/

QEMU VFIO PCI passthrough traces and kernel logs captured during
reverse engineering. These are the primary source for the golden
init sequence and FPGA bitbang protocol.

| File                  | Size   | Description                          |
|-----------------------|--------|--------------------------------------|
| `motu_hw_trace.log`   | 60 MB  | Full QEMU VFIO trace (primary)       |
| `motu_hw_trace2.log`  | 62 MB  | Second QEMU VFIO trace capture       |
| `post_init.txt`       | 29 KB  | Post-init port configuration trace   |
| `bitbang_head.txt`    | 5 KB   | FPGA bitbang values (head)            |
| `bitbang_sequence.txt`| 5 KB   | FPGA bitbang values (sequence)        |
| `DeviceIOControl.txt` | 1.2 MB | IOCTL trace from Windows driver       |
| `dsp_mailbox_init.txt`| 447 KB | DSP mailbox initialization trace     |
| `extract_dsp.txt`     | 284 KB | DSP program extraction trace          |
| `last_2000.txt`       | 117 KB | Last 2000 trace lines                 |
| `outsl_dump.txt`      | 169 KB | outsl DMA dump                        |
| `dmesg_output.txt`    | 2 KB   | Kernel dmesg during testing           |
| `test_trace.log`      | 11 KB  | Test run trace                        |
| `fpgu_cold_boot.txt`  | 1 KB   | Cold boot strategy notes              |

## disassembly/

Disassembly of the Windows driver and applications, used for
[reverse engineering](../re/re_strategy.md).

| File                          | Size   | Description                     |
|-------------------------------|--------|---------------------------------|
| `motuaw.asm`                  | 5.5 MB | Windows driver disassembly       |
| `cuemix.exe.asm`              | 229 MB | CueMix FX application           |
| `MOTU PCI Audio Console.exe.asm`| 117 MB | Audio Console application     |
| `motuaw_init.asm`             | 7 KB   | Init function disassembly       |
| `motuaw_init_true.asm`        | 5 KB   | True init function              |
| `ioctl_switch.asm`            | 31 KB  | IOCTL dispatch switch           |
| `strings_drv.txt`             | 23 KB  | Strings from motuaw.sys         |
| `strings_app.txt`             | 117 KB | Strings from applications      |
| `imports.txt`                 | 1 KB   | Driver import table              |

## binaries/

Compiled RE tools and prototype kernel modules. Source code for these
is in [../source/tools/](../tools/).

### Userspace Tools

| Binary                    | Purpose                              |
|---------------------------|--------------------------------------|
| `poke_fpga`               | FPGA bitbang + DSP load (working!)   |
| `poke_test` - `poke_test11` | Register poke tests                |
| `motu_poke`               | Userspace poke tool                  |
| `smart_replay`            | Golden sequence replay with DMA xlate|
| `replay_trace`            | Trace replay tool                    |
| `fix_bitbang`             | Bitbang sequence fixer              |
| `motu_clock_sync`         | Clock sync test                      |
| `motu_sync_finder` - `3`  | Sync register pattern finder        |
| `motu_clock_exhaustive`   | Exhaustive clock register testing   |
| `motu_force_sync`         | Force sync without waiting          |
| `motu_monitor`            | Monitor sync status                 |
| `motu_safe_sync`          | Safe sync (no hardware risk)        |
| `motu_test_amcc`          | AMCC register test                  |
| `motu_test_bar0`          | BAR0 DSP memory test                |
| `motu_test_regs`          | BAR1 register test                   |

### Kernel Modules

| Module                    | Purpose                              |
|---------------------------|--------------------------------------|
| `motu_pci_alsa.ko`        | Prototype ALSA driver                 |
| `motu_poke_driver.ko`     | Poke driver (register access)        |

## docs/

Strategy documents and reference output from the development process.

| File                          | Description                          |
|-------------------------------|--------------------------------------|
| `motu_re_strategy.md`         | Overall RE strategy                  |
| `driver_development_strategy.md` | Driver development plan           |
| `motu-prompt.txt`             | Original project prompt              |
| `lspci_output.txt`            | `lspci -v` output                    |
| `lspci_n_output.txt`          | `lspci -n` output (numeric IDs)      |
| `instruct`                    | Development instructions             |

## firmware/

Firmware blobs extracted from the Windows driver and QEMU traces.

| File                     | Size   | Description                        |
|--------------------------|--------|------------------------------------|
| `pci_424_fw.bin`         | 37 KB  | Altera FPGA bitstream (.rbf)      |
| `pci_424_dsp.bin`        | 64 KB  | DSP program (full)                 |
| `pci_424_dsp2.bin`       | 25 KB  | DSP program (partial)              |
| `pci_424_fw_full.bin`    | 177 KB | Full firmware blob                 |
| `motu_firmware_blob.bin` | 177 KB | Firmware blob (renamed)            |

## test/

| File           | Size   | Description                     |
|----------------|--------|---------------------------------|
| `test.wav`     | 576 KB | Test audio (16-bit, 44.1 kHz)   |
| `test_s32.wav` | 768 KB | Test audio (32-bit, 48 kHz)     |

## build/

Kernel module build artifacts (`.cmd` files, `Module.symvers`, etc.)
from the prototype driver builds.

## misc/

| File                | Description                    |
|---------------------|--------------------------------|
| `qemu-trace-events` | QEMU trace event definitions   |

# See Also

- [RE Strategy](../re/re_strategy.md)
- [Golden Sequence](../re/golden/golden_sequence.md)
- [FPGA Bitbang Protocol](../re/fpga/bitbang_protocol.md)
- [24I/O Init Failure](../driver/modules/24io_init_failure.md)
