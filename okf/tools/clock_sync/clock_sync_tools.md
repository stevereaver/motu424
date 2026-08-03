---
type: Tool
title: Clock Sync Tools
description: Tools for testing clock synchronization and finding the correct register patterns for clock lock.
tags: [tools, clock, sync, hardware]
generated: { by: human:reaver, at: 2026-03-08T10:30:00Z }
sources:
  - id: clock-sync
    resource: ../source/tools/clock_sync/motu_clock_sync.c
    title: motu_clock_sync.c
  - id: sync-finder
    resource: ../source/tools/clock_sync/motu_sync_finder.c
    title: motu_sync_finder.c
  - id: safe-sync
    resource: ../source/tools/clock_sync/motu_safe_sync.c
    title: motu_safe_sync.c
---

# Overview

Multiple tools were developed to find the correct register patterns
for achieving clock synchronization on the MOTU PCI-424. These tools
systematically test different register banks and bit patterns.

# Tools

| Tool                          | Purpose                              |
|-------------------------------|--------------------------------------|
| `motu_clock_sync.c`           | Basic clock sync test                |
| `motu_sync_finder.c`          | Search for sync register patterns    |
| `motu_sync_finder2.c`         | Extended sync finder                 |
| `motu_safe_sync.c`            | Safe sync (no hardware damage risk)  |
| `motu_safe_sync_bit0.c`       | Test bit 0 of sync register          |
| `motu_safe_sync_port.c`       | Test port-based sync                 |
| `motu_force_sync.c`           | Force sync without waiting           |
| `motu_monitor.c`              | Monitor sync status                  |
| `motu_clock_exhaustive.c`     | Exhaustive register testing          |
| `motu_clock_exhaustive_banks.c`| Exhaustive bank testing             |

# Key Findings

- The sync status is readable at [BAR0 offset 0x70d0](/registers/sync_status.md)
  and [BAR2 offset 0x00](/hardware/bars/bar2_fpga_port.md)
- A status of `0x13` at BAR2 indicates FPGA configured + clock locked
- The card may need up to 2 seconds to achieve lock after init

# See Also

- [Sync Status Registers](/registers/sync_status.md)
- [Sample Rates](/audio/sample_rates.md)
- [Init Sequence Replay](/driver/modules/init_replay.md)
