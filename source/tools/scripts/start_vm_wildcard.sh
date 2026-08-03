#!/bin/bash

# Ensure we have unlimited locked memory for VFIO
ulimit -l unlimited

# Ensure local X11 connections are allowed for root
xhost +local:root 2>/dev/null

echo "Starting Windows VM with PCIe Passthrough and Hardware Tracing..."
echo "Trace will be saved to motu_hw_trace.log"

sudo qemu-system-x86_64 -enable-kvm -m 4096 -cpu host \
  -drive file=win10.qcow2,format=qcow2 \
  -cdrom /path/to/your/windows10.iso \
  -device vfio-pci,host=06:01.0 \
  -trace vfio_region* 2> motu_hw_trace.log

echo "VM has shut down. Check motu_hw_trace.log!"
