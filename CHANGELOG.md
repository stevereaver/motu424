# Changelog

All notable changes to the MOTU PCI-424 driver project are documented
in this file.

## [Unreleased]

## [0.3.0] - 2026-08-03

### Added
- Top-level `Makefile` with convenience targets (`linux`, `windows`,
  `fw`, `tools`, `clean`, `help`)
- Unified `poke.c` tool consolidating 16 iterative poke test programs
  into a single tool with subcommands: `read`, `write`, `scan`,
  `monitor`, `fpga`
- `Makefile` and `README.md` for the poke tool
- GitHub Actions CI workflow (`.github/workflows/ci.yml`) with 5 jobs:
  Linux kernel module build, init sequence regeneration verification,
  Python syntax check, SPDX header check, Windows driver build
- `CONTRIBUTING.md` with contribution guidelines and code style
- `CHANGELOG.md` (this file)

### Changed
- Added SPDX-License-Identifier headers to all 50 C/H source files
- Updated README with kernel-headers package names and top-level
  make targets
- Updated OKF `poke_driver.md` to document the unified poke tool

### Removed
- Duplicated `source/driver/` directory (superseded by `source/shared/`
  + `source/linux/`). All OKF docs updated to canonical locations.

### Archived
- 16 iterative poke test programs moved to `source/tools/poke/archive/`

## [0.2.0] - 2026-08-03

### Added
- Windows WDF driver (`motu424.sys`, 17 KB) with IOCTL interface:
  `GET_INFO`, `START`, `STOP`, `GET_POSITION`, `READ_REG`, `WRITE_REG`,
  `GET_STATUS`
- Windows PAL implementation (`motu424_win_pal.c`) using WDF APIs:
  `MmMapIoSpaceEx`, `WdfCommonBuffer`, `WdfInterruptCreate`
- Userspace test tool (`motu424_test.exe`, 148 KB) with 12 built-in tests
- Windows build script (`build.bat`) using `vcvarsall.bat` + `cl.exe`
  + `link.exe` directly (no MSBuild project needed)
- Driver installation INF supporting PCI device IDs 0x0003, 0x0004,
  0x0005 with KMDF 1.15
- PowerShell test suite (`test.ps1`) with 16 tests
- Driver installation scripts (`install.bat`, `install.ps1`)
- OKF documentation for Windows build process

### Changed
- Shared headers updated for MSVC compatibility: `#pragma pack(push,1)`
  for packed structs, conditional `__attribute__((format(printf,...)))`
  macro, `pal_irq_handler_t` typedef reordered before use in
  `pal_device`
- Windows PAL logging adapted to use `vDbgPrintExWithPrefix` for
  kernel-safe output (no UCRT dependency)

## [0.1.0] - 2026-03-08

### Added
- Clean-room ALSA driver for Linux with FPGA loading, init sequence
  replay, DMA management, and PCM operations
- Cross-platform architecture: `source/shared/` (PAL + hardware core),
  `source/linux/` (ALSA frontend), `source/windows/` (WDF frontend)
- Platform Abstraction Layer abstracting PCI enable, BAR mapping,
  MMIO read/write, DMA allocation, IRQ, firmware loading, timing,
  logging, and spinlocks
- Golden initialization sequence (7192 register writes) captured via
  QEMU VFIO PCI passthrough tracing
- FPGA bitstream loading via Altera bitbang protocol
- DMA buffer management with scatter-gather setup
- Reverse-engineering tools: golden sequences, poke programs, trace
  replay, clock sync, analysis scripts (103 Python scripts)
- OKF documentation bundle: hardware docs, register maps, audio
  formats, driver architecture, RE methodology

### Fixed
- 24I/O audio interface initialization failure: corrected FPGA bitbang
  protocol (was completely wrong), added missing DSP kick-start, added
  post-init port configuration loop, added DSP RAM zeroing

### Notes
- Driver developed through clean-room reverse engineering using QEMU
  VFIO PCI passthrough tracing of the original Windows driver. No
  proprietary source code was used.
