---
type: Software Module
title: PCI Probe and Card Setup
description: PCI device enumeration, BAR mapping, ALSA card creation, and module entry/exit.
tags: [driver, pci, alsa, probe]
generated: { by: human:reaver, at: 2026-03-08T10:30:00Z }
sources:
  - id: main-src
    resource: ../source/driver/motu424_main.c
    title: motu424_main.c
---

# Overview

`motu424_main.c` handles PCI device probing, BAR mapping, ALSA card
creation, interrupt registration, and module entry/exit.

# PCI ID Table

```c
static const struct pci_device_id motu424_ids[] = {
    { PCI_DEVICE(0x137a, 0x0004) },
    { 0, }
};
```

# Probe Sequence

1. `snd_card_new()` — Allocate ALSA card with `motu424` struct as private data
2. `pci_enable_device()` + `pci_set_master()` — Enable PCI and bus mastering
3. `dma_set_mask_and_coherent()` — Set 32-bit DMA mask
4. `pci_request_regions()` + `pci_iomap()` — Map BAR0/1/2
5. `motu424_dma_alloc()` — Allocate coherent DMA buffer
6. `motu424_load_fpga()` — Load [FPGA bitstream](/driver/modules/fpga_loading.md)
7. `motu424_replay_sequence()` — [Replay init sequence](/driver/modules/init_replay.md)
8. `motu424_hw_init()` — [Sync polling](/registers/sync_status.md)
9. `request_irq()` — Register [IRQ handler](/driver/modules/irq_handler.md)
10. `motu424_pcm_new()` — Create [PCM device](/driver/modules/pcm_operations.md)
11. `snd_card_register()` — Register with ALSA

# Error Handling

All error paths goto `err_card`, which calls `snd_card_free()`. This
triggers `motu424_free()` (the `private_free` callback) which handles
all cleanup with null checks for partially-initialized state.

# Cleanup

`motu424_free()` disables interrupts, stops DMA, frees IRQ, frees DMA
buffer, unmaps BARs, releases PCI regions, and disables the PCI device.

# See Also

- [Driver Overview](/driver/driver_overview.md)
- [IRQ Handler](/driver/modules/irq_handler.md)
