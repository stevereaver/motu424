import struct
import pefile

pe = pefile.PE("windows-drv/motuaw.sys")
base = pe.OPTIONAL_HEADER.ImageBase
target_addr = base + 0x730D0
target_rva = 0x730D0

with open("windows-drv/motuaw.sys", "rb") as f:
    data = f.read()

for i in range(0, len(data)-8):
    val64 = struct.unpack("<Q", data[i:i+8])[0]
    if val64 == target_addr:
        print(f"Found 64-bit end pointer at offset 0x{i:X}")
    val32 = struct.unpack("<I", data[i:i+4])[0]
    if val32 == target_rva:
        print(f"Found 32-bit end RVA at offset 0x{i:X}")
