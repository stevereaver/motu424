---
type: Methodology
title: CueMix FX Mixer Tracing
description: Procedure for tracing CueMix FX mixer operations through QEMU VFIO to map DSP memory to mixer controls.
tags: [re, mixer, cuemix, qemu, vfio, tracing]
generated: { by: human:reaver, at: 2026-08-03T12:00:00Z }
sources:
  - id: trace-script
    resource: ../source/tools/scripts/trace_cuemix.sh
    title: QEMU tracing script
  - id: analyze-script
    resource: ../source/tools/analysis/analyze_mixer_trace.py
    title: Mixer trace analysis script
  - id: deviceio-trace
    resource: development/traces/DeviceIOControl.txt
    title: Existing DeviceIoControl trace from CueMix FX
  - id: post-init-trace
    resource: development/traces/post_init.txt
    title: Post-init BAR0 writes showing routing matrix
---

# Overview

CueMix FX is the MOTU control application that provides a GUI for
mixer routing, volume control, mute, phantom power, and clock source
selection. It communicates with the `motuaw.sys` kernel driver via
`DeviceIoControl` with IOCTL code `0x0022200f`.

The driver translates these IOCTL commands into BAR0 (DSP memory)
and BAR1 (control register) writes. By tracing the VFIO PCI accesses
while performing specific mixer actions, we can map which DSP memory
addresses control which mixer parameters.

# What We Already Know

From the existing `DeviceIOControl.txt` trace (7,621 calls captured
via API Monitor):

- **IOCTL code**: `0x0022200f` (all mixer commands use this single code)
- **Input buffer**: 302 bytes (0x12e) — contains the mixer command
- **Output buffer**: 4 bytes — likely a status return

From the `post_init.txt` VFIO trace:

- **Routing matrix**: BAR0 `0x8348-0x835C` — 24-channel identity mapping
  (6 dwords, 4 bytes each, little-endian channel indices)
- **Routing padding**: BAR0 `0x8360-0x83A4` — zeros (unused routing slots)
- **DSP DMA engine**: BAR0 `0x7000-0x70FF` — DMA buffer descriptors
- **DSP mailbox**: BAR0 `0x6FEC` — zeroed (mailbox clear)

# Prerequisites

1. **Linux host** with the MOTU PCI-424 card installed
2. **QEMU/KVM** with VFIO PCI passthrough configured
3. **Windows 10 guest VM** with:
   - Original MOTU driver (`motuaw.sys`) installed
   - CueMix FX installed (`CueMix FX.exe`)
   - API Monitor (optional, for correlating IOCTL calls)
4. The MOTU card must be bound to `vfio-pci` on the host

# Tracing Procedure

## Step 1: Prepare the Host

Bind the MOTU card to VFIO:

```bash
# Find the MOTU card PCI address
lspci -nn | grep 137a

# Bind to vfio-pci (adjust address as needed)
echo "0000:06:01.0" | sudo tee /sys/bus/pci/devices/0000:06:01.0/driver/unbind
echo "vfio-pci" | sudo tee /sys/bus/pci/devices/0000:06:01.0/driver_override
echo "0000:06:01.0" | sudo tee /sys/bus/pci/drivers/vfio-pci/bind
```

## Step 2: Start the VM with Tracing

```bash
cd source/tools/scripts
chmod +x trace_cuemix.sh
./trace_cuemix.sh
```

This starts QEMU with VFIO tracing enabled. The trace will be saved
to `cuemix_trace_<timestamp>.log`.

## Step 3: Wait for Windows to Boot

Wait for the Windows VM to fully boot and the MOTU driver to
initialize. You should see the MOTU card in Device Manager.

## Step 4: Establish a Baseline

**Do NOT open CueMix FX yet.** Let the system sit idle for 30 seconds
after boot to establish a baseline of "no mixer activity" in the
trace. This baseline can be used to filter out periodic status polling.

## Step 5: Open CueMix FX

Launch `CueMix FX.exe` in the VM. Wait for it to fully load and
display the mixer interface.

**Note the time** — this is your "CueMix opened" marker. Any writes
after this point are mixer-related.

## Step 6: Perform Mixer Actions (One at a Time)

Perform each action below, noting the exact time you perform it.
Wait 5-10 seconds between actions so they can be clearly separated
in the trace.

### Action 1: Volume Change
1. Note the current time
2. Adjust the volume slider on channel 1 from 0 to -6 dB
3. Wait 5 seconds
4. Adjust to -12 dB
5. Wait 5 seconds
6. Adjust back to 0 dB
7. Wait 5 seconds

### Action 2: Mute Toggle
1. Note the time
2. Click mute on channel 1
3. Wait 5 seconds
4. Unmute channel 1
5. Wait 5 seconds

### Action 3: Routing Change
1. Note the time
2. Change the routing for output 1 to route input 1 → output 1
3. Wait 5 seconds
4. Change routing to input 2 → output 1
5. Wait 5 seconds
6. Reset to default routing
7. Wait 5 seconds

### Action 4: Phantom Power
1. Note the time
2. Enable phantom power on input 1
3. Wait 5 seconds
4. Disable phantom power
5. Wait 5 seconds

### Action 5: Clock Source
1. Note the time
2. Change clock source from internal to external (if available)
3. Wait 5 seconds
4. Change back to internal
5. Wait 5 seconds

### Action 6: Sample Rate
1. Note the time
2. Change sample rate from 48 kHz to 44.1 kHz
3. Wait 5 seconds
4. Change back to 48 kHz
5. Wait 5 seconds

## Step 7: Close CueMix FX and Shut Down

1. Close CueMix FX
2. Shut down the Windows VM normally
3. The trace script will report the output file

## Step 8: Analyze the Trace

```bash
# Full analysis
python3 source/tools/analysis/analyze_mixer_trace.py cuemix_trace_*.log

# Show only mixer writes (BAR0 0x6000-0x10000)
python3 source/tools/analysis/analyze_mixer_trace.py cuemix_trace_*.log --bar 0 --range 0x6000 0x10000

# Diff against init trace to isolate mixer-only writes
python3 source/tools/analysis/analyze_mixer_trace.py cuemix_trace_*.log \
    --diff development/traces/motu_hw_trace.log
```

# Correlating with DeviceIoControl

If you also ran API Monitor on the guest, you can correlate the
DeviceIoControl calls with the VFIO trace:

1. Export the API Monitor log as CSV
2. Note the timestamps of each DeviceIoControl call
3. Match them to the VFIO trace timeline
4. The writes that follow each IOCTL call are the register operations
   triggered by that mixer command

# Expected Results

After analysis, you should have:

| Mixer Action | BAR | Offset | Value Pattern |
|-------------|-----|--------|-------------|
| Volume ch1  | BAR0 | TBD | TBD |
| Mute ch1    | BAR0 | TBD | TBD |
| Routing     | BAR0 | 0x8348-0x835C | Channel index bytes |
| Phantom pwr | BAR1 | TBD | TBD |
| Clock src   | BAR1 | TBD | TBD |
| Sample rate | BAR1 | 0x20 | Rate value |

The routing matrix at `0x8348-0x835C` is already mapped. The volume
and mute addresses need to be discovered through tracing.

# See Also

- [QEMU VFIO Tracing](/re/tracing/qemu_vfio.md)
- [IOCTL Handler Analysis](/re/ioctl/ioctl_analysis.md)
- [Golden Sequence](/re/golden/golden_sequence.md)
- [Control Registers](/registers/control_registers.md)
