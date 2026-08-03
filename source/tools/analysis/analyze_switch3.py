import sys
import struct
from capstone import *
from capstone.x86 import *
import pefile

def analyze(filepath):
    pe = pefile.PE(filepath)
    md = Cs(CS_ARCH_X86, CS_MODE_64)
    md.detail = True
    
    with open(filepath, 'rb') as f:
        data = f.read()
    
    # Let's find exactly where the jump table is
    jmp_address = 0x2779d
    rip_after_jmp = jmp_address + 7 # usually 7 bytes for jmp qword ptr [rip + disp]
    disp = 0xcdfc
    
    table_rva = rip_after_jmp + disp
    print(f"Jump table RVA: 0x{table_rva:x}")
    
    # We need to find this RVA in the sections
    table_offset = -1
    for section in pe.sections:
        if section.VirtualAddress <= table_rva < section.VirtualAddress + section.SizeOfRawData:
            table_offset = section.PointerToRawData + (table_rva - section.VirtualAddress)
            print(f"Table offset in file: 0x{table_offset:x}")
            break
            
    if table_offset != -1:
        print("Jump table entries:")
        # Let's read a few entries (QWORDs)
        for i in range(16):
            entry_bytes = data[table_offset + i*4 : table_offset + (i+1)*4] # wait, is it an array of 32-bit offsets?
            if len(entry_bytes) == 4:
                entry = struct.unpack('<I', entry_bytes)[0]
                # AMD64 PE jump tables are often 32-bit offsets from the image base
                target_rva = entry # + image_base ?
                print(f"Case {i}: offset 0x{entry:x}")
                
if __name__ == '__main__':
    analyze(sys.argv[1])
