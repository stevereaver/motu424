# Contributing

## Code Style

### C Code
- Use the existing code style in the repository
- Kernel-mode code: follow Linux kernel style (for Linux) or WDF
  conventions (for Windows)
- Shared code (`source/shared/`) must compile on both MSVC and GCC
- Use the PAL interface for all hardware access — never call OS-specific
  APIs directly from shared code

### Adding New Hardware Support

To add support for a new MOTU PCI-424 variant:

1. Add the PCI vendor/device ID to:
   - `source/shared/motu424_hw.h` (device ID constants)
   - `source/linux/motu424_alsa.c` (ALSA PCI ID table)
   - `source/windows/motu424.inf` (INF PnP ID section)

2. If the hardware requires a different FPGA bitstream, add it to
   `source/shared/` and update the PAL firmware loading path.

3. If the init sequence differs, generate a new golden trace and
   regenerate `init_sequence.bin` using `convert_golden.py`.

### Documentation

All documentation uses the Open Knowledge Format (OKF). See
`okf/index.md` for the documentation structure. When adding new
documentation:

1. Place it in the appropriate `okf/` subdirectory
2. Use the OKF frontmatter format (see existing files for examples)
3. Update the relevant `index.md` if one exists

## Testing

### Before Submitting Changes

**Linux driver:**
```bash
cd source/linux/
make
# Load and verify on hardware if available
```

**Windows driver:**
```bash
source\windows\build.bat clean
source\windows\build.bat
# Verify motu424.sys and motu424_test.exe are produced
```

### Test Tool

The Windows test tool (`motu424_test.exe`) can be used to verify basic
driver functionality without audio hardware:
```
motu424_test.exe info     # Query device info
motu424_test.exe test     # Run built-in tests
```

## Pull Requests

1. Fork the repository
2. Create a feature branch: `git checkout -b feature/my-feature`
3. Make your changes
4. Verify the build works on your platform
5. Commit with a clear message (see existing commit history for style)
6. Open a pull request

## Reporting Issues

When reporting bugs, please include:
- Platform (Linux/Windows) and version
- Kernel version (Linux) or Windows build number
- Hardware model (24I/O, 2408, etc.)
- `dmesg` output (Linux) or Driver Verifier log (Windows)
- Steps to reproduce
