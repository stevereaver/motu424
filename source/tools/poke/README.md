# Poke Tools

Userspace tools for exploring MOTU PCI-424 hardware registers via the
`motu_poke_driver` kernel module.

## Prerequisites

1. Load the poke driver kernel module:
   ```bash
   cd ../drivers
   make
   sudo insmod motu_poke_driver.ko
   # Creates /dev/motu_poke
   ```

2. Build the poke tool:
   ```bash
   make
   ```

## Usage

```
poke read    <bar> <offset>              Read a 32-bit register
poke write   <bar> <offset> <value>      Write a 32-bit register
poke scan    <bar> [start] [end]          Scan a BAR for non-zero values
poke monitor <bar> <offset> [count]       Monitor a register (read repeatedly)
poke fpga    <firmware.rbf>              Load FPGA bitstream + DSP program
```

### Bar Mapping

| Bar | Region | Size    | Description                  |
|-----|--------|---------|------------------------------|
| 0   | BAR0   | 4 MB    | DSP program/data memory     |
| 1   | BAR1   | 8 MB    | Control registers           |
| 2   | BAR2   | 16 B    | FPGA configuration port     |

### Examples

```bash
# Read the global config register
./poke read 1 0x00

# Write to the clock/rate register
./poke write 1 0x20 0x00175f3f

# Scan BAR1 for non-zero registers
./poke scan 1 0x0 0x100000

# Monitor the sync status register
./poke monitor 1 0x1C 50

# Load FPGA firmware and boot DSP
./poke fpga ../../shared/altera424b.rbf
```

## Archived Files

The `archive/` subdirectory contains earlier iterative versions of the
poke tool that were used during reverse engineering. They are kept for
historical reference but are superseded by the unified `poke.c` tool.
