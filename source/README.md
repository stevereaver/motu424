# MOTU PCI-424 Driver — Source Code

Unified cross-platform driver for the MOTU PCI-424 audio interface, with
a shared hardware core and platform-specific frontends for Linux (ALSA)
and Windows (WDF).

## Layout

```
source/
├── shared/              Cross-platform hardware logic (the "brain")
│   ├── motu424_pal.h    Platform Abstraction Layer interface
│   ├── motu424_hw.h     Hardware definitions, register maps, constants
│   ├── motu424_fpga.c   FPGA bitstream loading (uses PAL)
│   ├── motu424_init.c   Init sequence replay + DMA translation (uses PAL)
│   ├── motu424_dma.c    DMA buffer management + SG setup (uses PAL)
│   ├── convert_golden.py  Regenerates init_sequence.bin from golden trace
│   ├── init_sequence.bin  Compact binary init sequence (7192 writes)
│   └── altera424b.rbf     Altera FPGA bitstream firmware
├── linux/               Linux ALSA frontend
│   ├── motu424_linux_pal.c  Linux PAL implementation (pci_*, ioread32, etc.)
│   ├── motu424_alsa.c       ALSA card/PCM operations
│   └── Makefile
├── windows/             Windows WDF frontend
│   ├── motu424_win_pal.c    Windows PAL implementation (WDF, MmMapIoSpace)
│   ├── motu424_wdf.c        WDF driver entry + IOCTL interface
│   ├── motu424.inf          Driver installation INF
│   └── CMakeLists.txt       Build configuration for WDK
├── driver/              Original monolithic driver (kept for reference)
├── tools/
│   ├── drivers/         Prototype kernel drivers (motu_pci_alsa, motu_poke)
│   ├── poke/            Userspace poke/test programs
│   ├── replay/          Trace replay tools (smart_replay, replay_trace)
│   ├── clock_sync/      Clock synchronization and sync-finder tools
│   ├── analysis/        Python scripts for analyzing disassembly/traces
│   ├── scripts/         Shell scripts for VM setup and module reloading
│   └── golden/          Golden register-write sequences from QEMU VFIO trace
```

## Architecture

The driver uses a **shared core + PAL** architecture:

- **shared/** contains all hardware interaction logic (FPGA loading, init
  sequence replay, DMA setup, register definitions). This code is
  identical on both platforms and uses only the PAL interface.

- **linux/** and **windows/** each implement the PAL (wrapping their
  respective OS APIs) and the audio frontend (ALSA or WDF/IOCTL).

The PAL abstracts: PCI enable, BAR mapping, MMIO read/write, DMA
allocation, IRQ, firmware loading, timing, logging, and spinlocks.

## Building the Linux Driver

```bash
cd repo/source/linux/
make
sudo make install
sudo make install-fw   # installs firmware to /lib/firmware/motu424/
```

## Building the Windows Driver

Requires the Windows Driver Kit (WDK) and CMake:

```bash
cd repo/source/windows/
cmake -B build -S .
cmake --build build
# Output: motu424.sys, motu424.inf, altera424b.rbf, init_sequence.bin
```

Or use Visual Studio with the WDK project templates.

## Regenerating the Init Sequence

```bash
cd repo/source/shared/
python3 convert_golden.py ../tools/golden/golden_dsp.c ../tools/golden/golden_sequence.c init_sequence.bin
```

## Building the RE Tools

```bash
cd repo/source/tools/drivers/
make                  # builds prototype kernel modules

cd repo/source/tools/poke/
gcc -o poke_test poke_test.c
```
