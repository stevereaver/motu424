import pefile
import struct

pe = pefile.PE("windows-drv/motuaw.sys")
text_section = next(s for s in pe.sections if b".text" in s.Name)
text_data = text_section.get_data()
text_rva = text_section.VirtualAddress

print(f"Scanning .text for any RIP-relative pointers into the .data section near the firmware (0x37D00 - 0x38000)")

for i in range(len(text_data) - 4):
    rel32 = struct.unpack("<i", text_data[i:i+4])[0]
    inst_addr_rva = text_rva + i
    target = inst_addr_rva + 4 + rel32
    
    if 0x37000 <= target <= 0x39000:
        print(f"Found reference to {hex(target)} at file offset {hex(text_section.PointerToRawData + i)}, instruction RVA {hex(inst_addr_rva)}")
