import struct
import pefile

pe = pefile.PE("windows-drv/motuaw.sys")
base = pe.OPTIONAL_HEADER.ImageBase
target_rva = pe.get_rva_from_offset(144787)
target_va = base + target_rva

print(f"AW24IO.cpp is at RVA 0x{target_rva:X}")

for section in pe.sections:
    if section.Characteristics & 0x20000000:
        sec_data = section.get_data()
        sec_rva = section.VirtualAddress
        
        for i in range(len(sec_data) - 4):
            rel32 = struct.unpack("<i", sec_data[i:i+4])[0]
            inst_addr_rva = sec_rva + i
            target = inst_addr_rva + 4 + rel32
            if target == target_rva:
                print(f"Found relative pointer at inst RVA 0x{inst_addr_rva:X}")

