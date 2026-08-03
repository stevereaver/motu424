---
type: Reference
title: Windows Driver Build and Test
description: How to build, install, and test the MOTU PCI-424 Windows WDF driver.
tags: [windows, build, wdf, driver, test]
generated: { by: human:reaver, at: 2026-03-08T13:00:00Z }
sources:
  - id: build-script
    resource: ../source/windows/build.bat
    title: Build script for driver and test tool
  - id: wdf-driver
    resource: ../source/windows/motu424_wdf.c
    title: WDF driver source
  - id: win-pal
    resource: ../source/windows/motu424_win_pal.c
    title: Windows PAL implementation
  - id: test-tool
    resource: ../source/windows/motu424_test.c
    title: Userspace test tool
  - id: inf-file
    resource: ../source/windows/motu424.inf
    title: Driver INF file
---

# Overview

The Windows driver is a KMDF (Kernel Mode Driver Framework) driver that
provides an IOCTL interface for userspace audio bridge functionality.
It uses the same shared hardware core as the Linux ALSA driver via the
Platform Abstraction Layer (PAL).

# Build Prerequisites

- **Visual Studio 2022 BuildTools** with MSVC 14.44+
- **WDK 10.0.26100.0** (Windows Driver Kit)
- **KMDF 1.35** (included with WDK)
- Windows 10/11 x64

Install the WDK via winget:
```
winget install Microsoft.WindowsWDK.10.0.26100
```

# Building

The project must be accessible via a drive letter if on a network share
(UNC paths are not supported by `cmd.exe`). For a local clone, just
`cd` into the repo directory.

Then run the build script from the repo root:
```
source\windows\build.bat           # Build everything
source\windows\build.bat driver     # Build only the kernel driver
source\windows\build.bat test      # Build only the test tool
source\windows\build.bat clean     # Clean build output
```

# Build Output

Output goes to `build/windows/`:

| File               | Size  | Description                        |
|--------------------|-------|------------------------------------|
| `motu424.sys`      | 17 KB | Kernel-mode WDF driver             |
| `motu424_test.exe` | 148 KB| Userspace test tool                |
| `motu424.inf`      | 2 KB  | Driver installation INF            |
| `altera424b.rbf`   | 37 KB | FPGA bitstream firmware            |
| `init_sequence.bin`| 65 KB | Golden init sequence               |
| `install.bat`      | 3 KB  | Driver installation script         |
| `install.ps1`      | 4 KB  | PowerShell installation script     |
| `test.ps1`         | 6 KB  | PowerShell test suite              |

# Driver Architecture

The Windows driver consists of:

1. **WDF Driver** (`motu424_wdf.c`) — DriverEntry, device creation,
   resource parsing, IOCTL handler
2. **Windows PAL** (`motu424_win_pal.c`) — MMIO, DMA, IRQ, timing,
   logging via WDF/WDM APIs
3. **Shared Core** (`../source/shared/`) — FPGA loading, init replay,
   DMA management (same as Linux)

## IOCTL Interface

| IOCTL                    | Description                        |
|--------------------------|------------------------------------|
| `GET_INFO`               | Get rate, DMA state, port status   |
| `START` / `STOP`          | Start/stop DMA                     |
| `GET_POSITION`           | Read DMA position counter          |
| `READ_REG` / `WRITE_REG` | Read/write hardware registers      |
| `GET_STATUS`             | Read BAR2 port status              |

# Installation

## Driver Signing

Windows requires kernel-mode drivers to be signed. Since this is an
open-source project without a WHQL signature, you must enable **test
signing** mode to load the driver.

### Enabling Test Signing

Test signing allows loading drivers signed with a test certificate
(instead of a Microsoft-trusted WHQL certificate). This affects the
entire system, so a reboot is required.

1. Open an **elevated Command Prompt** (Run as Administrator)
2. Enable test signing:
   ```
   bcdedit /set testsigning on
   ```
3. Reboot:
   ```
   shutdown /r /t 0
   ```
4. After reboot, the desktop will show "Test Mode" in the bottom-right
   corner. This is normal.

To disable test signing later:
```
bcdedit /set testsigning off
shutdown /r /t 0
```

### Creating a Test Certificate

If you need to sign the driver yourself (e.g., for distribution within
your organization), create a test certificate:

```powershell
# Create a self-signed test certificate
$cert = New-SelfSignedCertificate `
    -Type CodeSigningCert `
    -Subject "CN=MOTU424 Test Signing" `
    -CertStoreLocation "Cert:\LocalMachine\My"

# Export it
$pwd = ConvertTo-SecureString -String "password" -Force -AsPlainText
Export-PfxCertificate -Cert $cert -FilePath motu424_test.pfx -Password $pwd

# Trust it (add to root and trusted publisher)
Import-PfxCertificate -FilePath motu424_test.pfx -Password $pwd `
    -CertStoreLocation "Cert:\LocalMachine\Root"
Import-PfxCertificate -FilePath motu424_test.pfx -Password $pwd `
    -CertStoreLocation "Cert:\LocalMachine\TrustedPublisher"
```

Then sign the driver:
```
signtool sign /a /v /fd SHA256 /f motu424_test.pfx /p password build\windows\motu424.sys
```

### Production / WHQL Signing

For production deployment without test signing, the driver must be
WHQL-signed through the Microsoft Partner Center. This requires a
hardware submission and is beyond the scope of this document. See
[Microsoft's WHQL documentation](https://learn.microsoft.com/en-us/windows-hardware/drivers/install/driver-signing).

## Installing the Driver

1. Ensure test signing is enabled (see above) and the system has been
   rebooted.

2. Run the installer as Administrator:
   ```
   cd build\windows
   install.bat
   ```

3. Or use PowerShell:
   ```
   .\install.ps1
   ```

4. Verify in Device Manager:
   - Open `devmgmt.msc`
   - Look under "Sound, video and game controllers" for "MOTU PCI-424"
   - If the device has a yellow exclamation mark, check the driver
     status for error codes

5. If the driver fails to load with error 39 (driver signing):
   ```
   # Verify test signing is enabled:
   bcdedit /enum | findstr testsigning

   # Should show: testsigning             Yes
   # If not, enable it and reboot again.
   ```

# Testing

## Quick Test

```
motu424_test.exe info       # Get device info
motu424_test.exe status     # Read BAR2 status
motu424_test.exe test       # Run full built-in test suite
```

## Full Test Suite

```
.\test.ps1
```

The test suite runs 16 tests:
- Pre-flight: test tool exists, service registered, device in Device Manager
- IOCTL: info, status, register reads, DMA start/stop/position
- Built-in: full test suite from the test tool

Tests that require hardware will report SKIP if the device is not present.

# See Also

- [Cross-Platform Architecture](../cross_platform_architecture.md)
- [Windows WDF Driver](windows_wdf.md)
- [24I/O Init Failure](24io_init_failure.md)
