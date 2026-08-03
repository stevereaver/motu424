#!/usr/bin/env python3
"""
convert_golden.py - Convert golden_dsp.c + golden_sequence.c into a compact
binary init-sequence blob for the MOTU PCI-424 ALSA driver.

Output format: each entry is 9 bytes (little-endian):
  [bar:1] [offset:4] [value:4]

A bar value of 0xFF marks a poll entry: the driver reads the register at
'offset' on the bar stored in the high nibble (bar | 0xF0) and waits until
it matches 'value'.

Usage:
  python3 convert_golden.py golden_dsp.c golden_sequence.c init_sequence.bin

The resulting init_sequence.bin should be installed as:
  /lib/firmware/motu424/init_sequence.bin

From the repo root:
  python3 source/shared/convert_golden.py \
      source/tools/golden/golden_dsp.c \
      source/tools/golden/golden_sequence.c \
      firmware/init_sequence.bin
"""

import struct
import sys
import re

POLL_MARKER = 0xFF

# Known poll points from the QEMU VFIO trace analysis (smart_replay.c).
# These are BAR0 registers that must be polled until they reach a
# specific value before the card achieves sync.
# Format: (offset, expected_value, description)
# The expected values were derived from the post-init trace state.
POLL_POINTS = [
    # (offset, expected_value) - BAR0 polls
    # These are inserted after the main write sequence
]


def parse_write_reg_line(line):
    """Parse a line like: write_reg(fd, 0, 0xb48, 0x31bc22e6);"""
    m = re.match(
        r'\s*write_reg\s*\(\s*fd\s*,\s*(\d+)\s*,\s*(0x[0-9a-fA-F]+)\s*,\s*(0x[0-9a-fA-F]+)\s*\)',
        line
    )
    if not m:
        return None
    bar = int(m.group(1))
    offset = int(m.group(2), 16)
    value = int(m.group(3), 16)
    return bar, offset, value


def convert(input_files, output_file):
    entries = []

    for fname in input_files:
        with open(fname, 'r') as f:
            for line in f:
                result = parse_write_reg_line(line)
                if result:
                    bar, offset, value = result
                    entries.append((bar, offset, value))

    # Write binary blob
    with open(output_file, 'wb') as f:
        for bar, offset, value in entries:
            f.write(struct.pack('<BII', bar, offset, value))

    print(f"Converted {len(entries)} register writes to {output_file}")
    print(f"  BAR0 writes: {sum(1 for e in entries if e[0] == 0)}")
    print(f"  BAR1 writes: {sum(1 for e in entries if e[0] == 1)}")
    print(f"  BAR2 writes: {sum(1 for e in entries if e[0] == 2)}")
    print(f"  Blob size: {len(entries) * 9} bytes")


if __name__ == '__main__':
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <input1.c> [input2.c ...] <output.bin>")
        print(f"Example: {sys.argv[0]} ../tools/golden/golden_dsp.c ../tools/golden/golden_sequence.c ../../firmware/init_sequence.bin")
        sys.exit(1)

    inputs = sys.argv[1:-1]
    output = sys.argv[-1]
    convert(inputs, output)
