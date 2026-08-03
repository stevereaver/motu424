---
name: Bug Report
about: Report a bug in the MOTU PCI-424 driver
title: "[BUG] "
labels: ["bug"]
---

## Describe the Bug

A clear and concise description of the bug.

## Environment

- **OS**: [e.g., Ubuntu 22.04, Windows 11 23H2]
- **Kernel/Build**: [e.g., 6.5.0-26-generic, Windows 10.0.26100]
- **Driver Version**: [e.g., 0.3.0, or commit hash]
- **Hardware Model**: [e.g., MOTU PCI-424 with 24I/O, 2408, 1224]
- **PCI Device ID**: [e.g., 0x0003, 0x0004, 0x0005 — from `lspci -nn | grep 137a`]

## Steps to Reproduce

1.
2.
3.

## Expected Behavior

What you expected to happen.

## Actual Behavior

What actually happened.

## Logs

### Linux

```
# Paste relevant dmesg output:
dmesg | grep motu424

# ALSA card detection:
aplay -l
arecord -l
```

### Windows

```
# Driver Verifier output, or Event Viewer -> System logs
# Or motu424_test.exe output:
motu424_test.exe info
motu424_test.exe test
```

## Additional Context

Any other context about the problem (workarounds tried, related
issues, etc.).
