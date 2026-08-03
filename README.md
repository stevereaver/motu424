# MOTU PCI-424 Audio Driver

A clean-room, cross-platform driver for the Mark of the Unicorn (MOTU)
PCI-424 audio interface card — a PCI card with an Altera FPGA and Motorola
DSP that provides multi-channel professional audio I/O.

## Status

| Platform | Status | Details |
|----------|--------|---------|
| Linux    | Working | ALSA driver with FPGA load, init replay, DMA, PCM |
| Windows  | Builds | WDF/KMDF driver with IOCTL interface (Phase 1) |

## Hardware

The MOTU PCI-424 is a PCI card (Vendor `0x137a`, Device `0x0003`/`0x0004`/`0x0005`)
with three BAR regions:

- **BAR0** — 4 MB prefetchable: Motorola DSP program/data memory
- **BAR1** — 8 MB non-prefetchable: Control registers
- **BAR2** — 16 bytes I/O port: FPGA configuration port

The card connects to external audio interface boxes (24I/O, 2408, etc.)
via a proprietary cable.

## Repository Layout

```
repo/
├── source/                 Driver source code
│   ├── shared/             Cross-platform hardware core (the "brain")
│   │   ├── motu424_pal.h   Platform Abstraction Layer interface
│   │   ├── motu424_hw.h    Hardware definitions, register maps
│   │   ├── motu424_fpga.c  FPGA bitstream loading (uses PAL)
│   │   ├── motu424_init.c  Init sequence replay + DMA translation
│   │   ├── motu424_dma.c   DMA buffer management + SG setup
│   │   ├── altera424b.rbf  Altera FPGA bitstream firmware
│   │   └── init_sequence.bin  Golden init sequence (7192 writes)
│   ├── linux/              Linux ALSA frontend
│   │   ├── motu424_linux_pal.c  Linux PAL (pci_*, ioread32, etc.)
│   │   ├── motu424_alsa.c       ALSA card/PCM operations
│   │   └── Makefile
│   ├── windows/            Windows WDF frontend
│   │   ├── motu424_win_pal.c    Windows PAL (WDF, MmMapIoSpace)
│   │   ├── motu424_wdf.c        WDF driver + IOCTL interface
│   │   ├── motu424_test.c       Userspace test tool
│   │   ├── motu424.inf          Driver installation INF
│   │   └── build.bat            Build script (cl.exe + link.exe)
│   └── tools/              Reverse-engineering and development tools
│       ├── golden/         Golden register-write sequences from QEMU trace
│       ├── poke/           Userspace poke/test programs
│       ├── replay/         Trace replay tools
│       ├── clock_sync/     Clock synchronization tools
│       ├── drivers/        Prototype kernel modules
│       ├── analysis/       Python scripts for trace analysis
│       └── scripts/        Shell scripts for VM setup
├── okf/                    Open Knowledge Format documentation bundle
│   ├── hardware/           Hardware documentation
│   ├── registers/          Register map documentation
│   ├── audio/              Audio format documentation
│   ├── driver/             Driver architecture documentation
│   ├── re/                 Reverse-engineering methodology
│   ├── tools/              Tool documentation
│   └── development/        Development artifacts reference
├── LICENSE                 GPL-2.0
└── README.md               This file
```

## Architecture

The driver uses a **shared core + PAL** architecture:

- **`source/shared/`** contains all hardware interaction logic (FPGA
  loading, init sequence replay, DMA setup, register definitions).
  This code is identical on both platforms and uses only the PAL
  interface.

- **`source/linux/`** and **`source/windows/`** each implement the PAL
  (wrapping their respective OS APIs) and the audio frontend
  (ALSA or WDF/IOCTL).

The PAL abstracts: PCI enable, BAR mapping, MMIO read/write, DMA
allocation, IRQ, firmware loading, timing, logging, and spinlocks.

## Building

### Linux Driver

Prerequisites:
- Linux kernel headers (for your kernel version)
- GCC
- make

```bash
cd source/linux/
make
sudo make install
sudo make install-fw   # installs firmware to /lib/firmware/motu424/
```

### Windows Driver

Prerequisites:
- Visual Studio 2022 BuildTools with C++ workload
- Windows Driver Kit (WDK) 10.0.26100.0 or later
- KMDF 1.35 (included with WDK)

Install the WDK:
```
winget install Microsoft.WindowsWDK.10.0.26100
```

Build:
```
source\windows\build.bat           # Build everything
source\windows\build.bat driver     # Build only the kernel driver
source\windows\build.bat test      # Build only the test tool
source\windows\build.bat clean     # Clean build output
```

Output goes to `build/windows/`.

See [okf/driver/modules/windows_build.md](okf/driver/modules/windows_build.md)
for detailed build and installation instructions.

### Regenerating the Init Sequence

The golden init sequence is derived from a QEMU VFIO trace of the
original Windows driver. To regenerate:

```bash
cd source/shared/
python3 convert_golden.py ../tools/golden/golden_dsp.c \
    ../tools/golden/golden_sequence.c init_sequence.bin
```

## Installation

### Linux

```bash
sudo modprobe snd-motu424
# Or: sudo insmod source/linux/motu_pci_alsa.ko
dmesg | grep motu424
aplay -l
```

### Windows

1. Enable test signing: `bcdedit /set testsigning on` (reboot required)
2. Run `build\windows\install.bat` as Administrator
3. Verify in Device Manager under "Sound, video and game controllers"

## Testing

### Linux

```bash
# Check card is recognized
aplay -l
arecord -l

# Play test audio
aplay -D hw:MOTU424 test.wav
```

### Windows

```
build\windows\motu424_test.exe info     # Get device info
build\windows\motu424_test.exe test     # Run full test suite
build\windows\test.ps1                 # PowerShell test suite
```

## Documentation

Full hardware and driver documentation is in the [OKF bundle](okf/index.md):

- [Hardware Documentation](okf/hardware/index.md)
- [Register Map](okf/registers/control_registers.md)
- [Driver Architecture](okf/driver/cross_platform_architecture.md)
- [Reverse Engineering Methodology](okf/re/re_strategy.md)
- [Windows Build Guide](okf/driver/modules/windows_build.md)

## License

GPL-2.0 — see [LICENSE](LICENSE).

## Acknowledgments

This driver was developed through clean-room reverse engineering of the
MOTU PCI-424 hardware using QEMU VFIO PCI passthrough tracing. No
proprietary source code was used. The golden initialization sequence was
captured by observing the original Windows driver's PCI transactions
through QEMU's tracing infrastructure.
