import struct
import pefile

pe = pefile.PE("windows-drv/motuaw.sys")
base = pe.OPTIONAL_HEADER.ImageBase
target_rva = 0x2EEFE
target_va = base + target_rva

print(f"Searching for RVA 0x{target_rva:X} and VA 0x{target_va:X}")

for section in pe.sections:
    data = section.get_data()
    for i in range(len(data) - 8):
        val64 = struct.unpack("<Q", data[i:i+8])[0]
        if val64 == target_va:
            file_off = section.PointerToRawData + i
            rva = section.VirtualAddress + i
            print(f"[{section.Name.decode()}] Found 64-bit VA pointer at file offset 0x{file_off:X}, RVA 0x{rva:X}")
        
        val32 = struct.unpack("<I", data[i:i+4])[0]
        if val32 == target_rva:
            file_off = section.PointerToRawData + i
            rva = section.VirtualAddress + i
            print(f"[{section.Name.decode()}] Found 32-bit RVA pointer at file offset 0x{file_off:X}, RVA 0x{rva:X}")



