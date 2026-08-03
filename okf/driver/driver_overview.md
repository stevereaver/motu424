---
type: Software Architecture
title: Driver Overview
description: Architecture of the clean-room ALSA kernel driver for the MOTU PCI-424.
tags: [driver, alsa, architecture, kernel-module]
generated: { by: human:reaver, at: 2026-03-08T10:30:00Z }
sources:
  - id: driver-src
    resource: ../source/linux/
    title: Driver source code
  - id: driver-readme
    resource: ../source/linux/README.md
    title: Driver README
---

# Module Structure

The driver (`snd_motu424`) is split into five source modules plus a
shared header:

| File              | Purpose                                              |
|-------------------|------------------------------------------------------|
| `motu424.h`       | Shared definitions: register map, structs, MMIO helpers |
| `motu424_main.c`  | [PCI probe/remove, module entry, ALSA card lifecycle](/driver/modules/pci_probe.md) |
| `motu424_fpga.c`  | [FPGA bitstream loading via bit-banged serial](/driver/modules/fpga_loading.md) |
| `motu424_init.c`  | [Golden-sequence replay + DMA translation + sync polling](/driver/modules/init_replay.md) |
| `motu424_dma.c`   | [Coherent DMA buffer allocation + SG descriptor setup](/driver/modules/dma_management.md) |
| `motu424_pcm.c`   | [ALSA PCM operations: open/close/prepare/trigger/pointer/ack](/driver/modules/pcm_operations.md) |

# Initialization Flow

On `probe()`, the driver executes:

1. **PCI enable** — `pci_enable_device()`, `pci_set_master()`, 32-bit DMA mask
2. **BAR mapping** — `pci_request_regions()` + `pci_iomap()` for BAR0/1/2
3. **DMA alloc** — Allocate coherent DMA buffer (must happen before init replay)
4. **FPGA load** — Bit-bang the Altera `.rbf` bitstream through BAR1
5. **Init sequence replay** — Write 7192 register values to BAR0 with DMA address translation
6. **Sync poll** — Poll status registers until clock lock
7. **IRQ request** — Register interrupt handler
8. **PCM create** — Register ALSA PCM device
9. **Card register** — `snd_card_register()`

# Build

```bash
cd ../source/linux/
make
sudo make install
sudo make install-fw
```

# Loading

```bash
sudo modprobe snd_motu424
dmesg | grep motu424
aplay -l
```

# See Also

- [PCI-424 Card](/hardware/pci424_card.md)
- [RE Strategy](/re/re_strategy.md)
