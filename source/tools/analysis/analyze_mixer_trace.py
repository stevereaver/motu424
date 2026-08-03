#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0
"""
analyze_mixer_trace.py - Analyze QEMU VFIO traces to isolate mixer writes.

Parses a QEMU VFIO trace log and separates the register writes into:
  1. Init sequence (known — can be filtered out)
  2. Mixer/routing writes (BAR0 DSP memory, 0x6000-0x8400 range)
  3. Control register writes (BAR1)
  4. FPGA port writes (BAR2)

Usage:
  python3 analyze_mixer_trace.py <trace.log>
  python3 analyze_mixer_trace.py <trace.log> --diff <init_trace.log>
  python3 analyze_mixer_trace.py <trace.log> --bar 0 --range 0x8000 0x8400

The --diff option subtracts a known init trace so only mixer-related
writes (those that happen AFTER initialization) are shown.
"""

import sys
import re
import argparse
from collections import defaultdict

# QEMU VFIO trace line format:
# vfio_region_write  (0000:06:01.0:region0+0x8348, 0x10203, 4)
# vfio_region_read  (0000:06:01.0:region0+0x6fb8, 4) = 0x30000

WRITE_RE = re.compile(
    r'vfio_region_write\s+\((\d+:\d+:\d+\.\d+):region(\d+)\+0x([0-9a-fA-F]+),\s+0x([0-9a-fA-F]+),\s+(\d+)\)'
)
READ_RE = re.compile(
    r'vfio_region_read\s+\((\d+:\d+:\d+\.\d+):region(\d+)\+0x([0-9a-fA-F]+),\s+(\d+)\)\s*=\s*0x([0-9a-fA-F]+)'
)

# BAR mapping
BAR_NAMES = {0: "BAR0 (DSP)", 1: "BAR1 (Regs)", 2: "BAR2 (FPGA)"}

# Known mixer/routing address ranges in BAR0
MIXER_RANGES = [
    (0x6000, 0x6FFF, "DSP mailbox/control"),
    (0x7000, 0x7FFF, "DSP DMA engine"),
    (0x8000, 0x8400, "Mixer routing matrix"),
    (0x8400, 0x10000, "Mixer parameters (volume/mute)"),
]


def parse_trace(filename):
    """Parse a QEMU VFIO trace file into a list of operations."""
    ops = []
    with open(filename, 'r') as f:
        for line in f:
            line = line.strip()
            m = WRITE_RE.match(line)
            if m:
                ops.append({
                    'type': 'write',
                    'time': m.group(1),
                    'bar': int(m.group(2)),
                    'offset': int(m.group(3), 16),
                    'value': int(m.group(4), 16),
                    'size': int(m.group(5)),
                })
                continue
            m = READ_RE.match(line)
            if m:
                ops.append({
                    'type': 'read',
                    'time': m.group(1),
                    'bar': int(m.group(2)),
                    'offset': int(m.group(3), 16),
                    'size': int(m.group(4)),
                    'value': int(m.group(5), 16),
                })
    return ops


def filter_by_bar(ops, bar):
    return [op for op in ops if op['bar'] == bar]


def filter_by_range(ops, start, end):
    return [op for op in ops if start <= op['offset'] <= end]


def subtract_ops(all_ops, init_ops):
    """Remove init_ops from all_ops (matching bar+offset+value for writes)."""
    init_writes = set()
    for op in init_ops:
        if op['type'] == 'write':
            init_writes.add((op['bar'], op['offset'], op['value']))

    result = []
    for op in all_ops:
        if op['type'] == 'write':
            key = (op['bar'], op['offset'], op['value'])
            if key not in init_writes:
                result.append(op)
        else:
            result.append(op)
    return result


def get_range_name(bar, offset):
    """Get a human-readable name for a BAR0 offset range."""
    if bar == 0:
        for start, end, name in MIXER_RANGES:
            if start <= offset <= end:
                return name
        if offset < 0x6000:
            return "DSP program memory"
        return "Unknown"
    elif bar == 1:
        return "Control register"
    elif bar == 2:
        return "FPGA port"
    return "Unknown"


def print_summary(ops):
    """Print a summary of the trace operations."""
    writes = [op for op in ops if op['type'] == 'write']
    reads = [op for op in ops if op['type'] == 'read']

    print(f"\n{'='*60}")
    print(f"  TRACE SUMMARY")
    print(f"{'='*60}")
    print(f"  Total operations: {len(ops)}")
    print(f"  Writes: {len(writes)}")
    print(f"  Reads:  {len(reads)}")
    print()

    # Group writes by BAR
    by_bar = defaultdict(int)
    for op in writes:
        by_bar[op['bar']] += 1
    print("  Writes by BAR:")
    for bar in sorted(by_bar.keys()):
        print(f"    {BAR_NAMES.get(bar, f'BAR{bar}')}: {by_bar[bar]} writes")
    print()

    # Group BAR0 writes by address range
    bar0_writes = filter_by_bar(writes, 0)
    if bar0_writes:
        print("  BAR0 writes by range:")
        for start, end, name in MIXER_RANGES:
            count = len(filter_by_range(bar0_writes, start, end))
            if count:
                print(f"    0x{start:04X}-0x{end:04X} ({name}): {count} writes")
        print()

    # Group BAR1 writes by offset
    bar1_writes = filter_by_bar(writes, 1)
    if bar1_writes:
        print("  BAR1 writes by offset:")
        by_offset = defaultdict(int)
        for op in bar1_writes:
            by_offset[op['offset']] += 1
        for offset in sorted(by_offset.keys()):
            print(f"    0x{offset:04X}: {by_offset[offset]} writes")
        print()


def print_mixer_writes(ops):
    """Print detailed mixer-related writes (BAR0, 0x6000-0x10000)."""
    mixer_ops = [op for op in ops
                 if op['type'] == 'write' and op['bar'] == 0
                 and 0x6000 <= op['offset'] <= 0x10000]

    if not mixer_ops:
        print("\n  No mixer writes found.")
        return

    print(f"\n{'='*60}")
    print(f"  MIXER WRITES (BAR0, 0x6000-0x10000)")
    print(f"{'='*60}")
    print(f"  Total: {len(mixer_ops)} writes")
    print()

    # Group by range
    for start, end, name in MIXER_RANGES:
        range_ops = [op for op in mixer_ops if start <= op['offset'] <= end]
        if range_ops:
            print(f"\n  --- {name} (0x{start:04X}-0x{end:04X}) ---")
            for op in range_ops:
                val_str = f"0x{op['value']:08X}"
                # Decode packed bytes for routing matrix
                if 0x8348 <= op['offset'] <= 0x835C:
                    bytes_val = [(op['value'] >> (i*8)) & 0xFF for i in range(4)]
                    val_str += f"  [ch: {bytes_val}]"
                print(f"    0x{op['offset']:04X} = {val_str}")


def print_routing_matrix(ops):
    """Print the routing matrix as a decoded table."""
    matrix_ops = [op for op in ops
                  if op['type'] == 'write' and op['bar'] == 0
                  and 0x8348 <= op['offset'] <= 0x83A4]

    if not matrix_ops:
        print("\n  No routing matrix writes found.")
        return

    print(f"\n{'='*60}")
    print(f"  ROUTING MATRIX (BAR0 0x8348-0x83A4)")
    print(f"{'='*60}")
    print()

    # Each dword contains 4 byte-sized channel indices
    channels = []
    for op in sorted(matrix_ops, key=lambda x: x['offset']):
        for i in range(4):
            ch = (op['value'] >> (i*8)) & 0xFF
            channels.append(ch)

    # Print as a table
    print(f"  {'Slot':<8} {'Channel':<10} {'Hex'}")
    print(f"  {'-'*8} {'-'*10} {'-'*4}")
    for i, ch in enumerate(channels):
        if ch == 0 and i >= 24:
            break
        print(f"  {i:<8} {ch:<10} 0x{ch:02X}")


def diff_traces(full_trace, init_trace):
    """Show only writes that are NOT in the init trace."""
    init_ops = parse_trace(init_trace)
    return subtract_ops(full_trace, init_ops)


def main():
    parser = argparse.ArgumentParser(
        description="Analyze QEMU VFIO traces for mixer writes"
    )
    parser.add_argument('trace', help='QEMU VFIO trace file')
    parser.add_argument('--diff', metavar='INIT_TRACE',
                        help='Subtract init trace to isolate mixer writes')
    parser.add_argument('--bar', type=int, choices=[0, 1, 2],
                        help='Filter by BAR number')
    parser.add_argument('--range', metavar=('START', 'END'), nargs=2,
                        help='Filter by address range (hex)')
    parser.add_argument('--summary-only', action='store_true',
                        help='Print summary only')
    args = parser.parse_args()

    ops = parse_trace(args.trace)
    print(f"\nParsed {len(ops)} operations from {args.trace}")

    if args.diff:
        print(f"Subtracting init trace: {args.diff}")
        ops = diff_traces(ops, args.diff)
        print(f"After diff: {len(ops)} operations remain")

    if args.bar is not None:
        ops = filter_by_bar(ops, args.bar)
        print(f"Filtered to {BAR_NAMES[args.bar]}: {len(ops)} operations")

    if args.range:
        start = int(args.range[0], 0)
        end = int(args.range[1], 0)
        ops = filter_by_range(ops, start, end)
        print(f"Filtered to 0x{start:04X}-0x{end:04X}: {len(ops)} operations")

    print_summary(ops)

    if not args.summary_only:
        print_mixer_writes(ops)
        print_routing_matrix(ops)

    print(f"\n{'='*60}")
    print("  Analysis complete.")
    print(f"{'='*60}")


if __name__ == '__main__':
    main()
