#!/bin/bash
# SPDX-License-Identifier: GPL-2.0
#
# trace_cuemix.sh - Start QEMU VM with VFIO tracing for CueMix FX mixer capture.
#
# This script starts a Windows VM with the MOTU PCI-424 card passed
# through via VFIO, with full PCI BAR access tracing enabled.
#
# The trace output is timestamped so mixer actions can be correlated
# with specific BAR writes.
#
# Usage:
#   ./trace_cuemix.sh [trace_output_file]
#
# Default output: cuemix_trace_<timestamp>.log

set -e

TRACE_FILE="${1:-cuemix_trace_$(date +%Y%m%d_%H%M%S).log}"
VM_IMAGE="${VM_IMAGE:-win10.qcow2}"
PCI_ADDR="${PCI_ADDR:-06:01.0}"
VM_MEM="${VM_MEM:-4096}"

# Ensure we have unlimited locked memory for VFIO
ulimit -l unlimited

# Allow X11 connections for root (for QEMU display)
xhost +local:root 2>/dev/null || true

echo "============================================"
echo "  MOTU PCI-424 CueMix FX Tracing"
echo "============================================"
echo ""
echo "  Trace output: $TRACE_FILE"
echo "  VM image:     $VM_IMAGE"
echo "  PCI address:  $PCI_ADDR"
echo "  VM memory:    ${VM_MEM} MB"
echo ""
echo "  Tracing: vfio_region_read + vfio_region_write"
echo ""
echo "============================================"
echo ""

# Create trace events file
EVENTS_FILE=$(mktemp /tmp/qemu-trace-events.XXXXXX)
cat > "$EVENTS_FILE" <<'EOF'
vfio_region_write
vfio_region_read
EOF

# Start QEMU with VFIO tracing
# The -trace flag sends trace output to stderr, which we redirect to our log
qemu-system-x86_64 \
  -enable-kvm \
  -m "$VM_MEM" \
  -cpu host \
  -drive file="$VM_IMAGE",format=qcow2 \
  -device vfio-pci,host="$PCI_ADDR",x-no-mmap=on \
  -trace events="$EVENTS_FILE" \
  -rtc base=localtime \
  -display gtk \
  2> "$TRACE_FILE"

# Clean up
rm -f "$EVENTS_FILE"

echo ""
echo "============================================"
echo "  VM shut down."
echo "  Trace saved to: $TRACE_FILE"
echo "  Trace size: $(du -h "$TRACE_FILE" | cut -f1)"
echo "============================================"
echo ""
echo "Next steps:"
echo "  1. Run the analysis script:"
echo "     python3 analyze_mixer_trace.py $TRACE_FILE"
echo "  2. Or compare with init sequence to isolate mixer writes:"
echo "     python3 analyze_mixer_trace.py $TRACE_FILE --diff motu_hw_trace.log"
