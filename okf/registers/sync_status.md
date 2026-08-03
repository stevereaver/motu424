---
type: Register
title: Sync Status Registers (BAR0 + 0x70d0)
description: Clock lock and synchronization status registers polled during initialization.
resource: pci:137a:0004:bar0:0x70d0
tags: [register, sync, clock, status]
generated: { by: human:reaver, at: 2026-03-08T10:30:00Z }
sources:
  - id: clock-sync
    resource: ../source/tools/clock_sync/motu_clock_sync.c
    title: Clock sync tool
  - id: sync-finder
    resource: ../source/tools/clock_sync/motu_sync_finder.c
    title: Sync finder tool
---

# Register Details

| Offset    | BAR  | Name           | Description                          |
|-----------|------|----------------|--------------------------------------|
| `0x70d0`  | BAR0 | Sync Status    | DSP clock/sync state                |
| `0x00`    | BAR2 | Port Status    | FPGA + clock lock status             |

# Port Status (BAR2 + 0x00)

The BAR2 status byte indicates overall card readiness:

| Value  | Meaning                              |
|--------|--------------------------------------|
| `0x13` | FPGA configured + clock locked      |
| Other  | Not ready (FPGA loading or no clock) |

The `MOTU_PORT_LOCK_STATUS` mask (`0x13`) checks bits 0, 1, and 4.

# Polling

During [hardware init](/driver/modules/init_replay.md), the driver
polls these registers with a 2-second timeout:

```c
err = motu424_poll_reg(motu, MOTU_BAR_DSP, MOTU_SYNC_REG_STATUS, 0x0, 2000);
```

The sync status register is polled for `0x0` (not busy), then the
BAR2 port status is checked for the `0x13` lock pattern.

# Clock Sync Tools

Multiple [clock sync tools](/tools/clock_sync/clock_sync_tools.md)
were developed to test different register banks and bit patterns
for achieving clock lock.

# See Also

- [Sample Rates](/audio/sample_rates.md)
- [Init Sequence Replay](/driver/modules/init_replay.md)
