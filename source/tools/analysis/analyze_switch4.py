import sys
from capstone import *
from capstone.x86 import *
import pefile
import struct

def analyze(filepath):
    pe = pefile.PE(filepath)
    md = Cs(CS_ARCH_X86, CS_MODE_64)
    md.detail = True
    
    with open(filepath, 'rb') as f:
        data = f.read()

    # The jump table seems to contain 32-bit RVAs relative to ImageBase
    image_base = pe.OPTIONAL_HEADER.ImageBase

    table_rva = 0x345a0
    table_offset = pe.get_offset_from_rva(table_rva)
    
    if table_offset != -1:
        for i in range(15):
            entry_bytes = data[table_offset + i*4 : table_offset + (i+1)*4]
            rva = struct.unpack('<I', entry_bytes)[0]
            # Disassemble the target
            target_offset = pe.get_offset_from_rva(rva)
            if target_offset == 0 or target_offset > len(data):
                 print(f"Case {i}: Invalid RVA 0x{rva:x}")
                 continue
                 
            print(f"\n--- Case {i} (target RVA 0x{rva:x}) ---")
            for inst in md.disasm(data[target_offset:target_offset+0x40], image_base + rva):
                print(f"0x{inst.address:x}:\t{inst.mnemonic}\t{inst.op_str}")
                if inst.id in (X86_INS_RET, X86_INS_JMP):
                    break

if __name__ == '__main__':
    analyze(sys.argv[1])
