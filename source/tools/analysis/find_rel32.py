import sys
import pefile
import struct

if len(sys.argv) < 2:
    print("Usage: find_rel32.py <rva>")
    sys.exit(1)

pe = pefile.PE("windows-drv/motuaw.sys")
target_rva = int(sys.argv[1], 16)

print(f"Searching for references to RVA {hex(target_rva)}")

for section in pe.sections:
    if section.Characteristics & 0x20000000:
        sec_data = section.get_data()
        sec_rva = section.VirtualAddress
        
        for i in range(len(sec_data) - 4):
            rel32 = struct.unpack("<i", sec_data[i:i+4])[0]
            inst_addr_rva = sec_rva + i
            target = inst_addr_rva + 4 + rel32
            if target == target_rva:
                print(f"Found reference at file offset {hex(section.PointerToRawData + i)}, inst RVA {hex(inst_addr_rva)}")
