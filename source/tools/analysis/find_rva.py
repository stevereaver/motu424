import sys
import pefile

if len(sys.argv) < 2:
    print("Usage: find_rva.py <offset>")
    sys.exit(1)

pe = pefile.PE("windows-drv/motuaw.sys")
offset = int(sys.argv[1])
rva = pe.get_rva_from_offset(offset)
print(f"Offset {offset} -> RVA: 0x{rva:X}, Virtual Address: 0x{pe.OPTIONAL_HEADER.ImageBase + rva:X}")
