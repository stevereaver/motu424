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

    image_base = pe.OPTIONAL_HEADER.ImageBase
    table_rva = 0x345a0
    table_offset = pe.get_offset_from_rva(table_rva)
    
    print(f"Table offset: 0x{table_offset:x}")
    if table_offset != -1:
        # In GCC/Clang/MSVC 64-bit jump tables are often 32-bit *relative* offsets to the table base or image base
        for i in range(15):
             entry_bytes = data[table_offset + i*4 : table_offset + i*4 + 4]
             offset_val = struct.unpack('<I', entry_bytes)[0]
             
             # Try relative to table base
             rva = table_rva + struct.unpack('<i', entry_bytes)[0]
             try:
                 target_offset = pe.get_offset_from_rva(rva)
                 print(f"\n--- Case {i} (target RVA 0x{rva:x}) ---")
             except Exception:
                 # Try relative to ImageBase (which is what standard RVA is)
                 rva = offset_val
                 try:
                     target_offset = pe.get_offset_from_rva(rva)
                     print(f"\n--- Case {i} (target RVA 0x{rva:x}) ---")
                 except Exception:
                     # Try absolute 64-bit address? Unlikely for PE
                     print(f"Case {i}: Cannot resolve offset 0x{offset_val:x}")
                     continue
                     
             try:
                 for inst in md.disasm(data[target_offset:target_offset+0x40], image_base + rva):
                     print(f"0x{inst.address:x}:\t{inst.mnemonic}\t{inst.op_str}")
                     if inst.id in (X86_INS_RET, X86_INS_JMP):
                         break
             except Exception as e:
                 print(f"Disasm error {e}")

if __name__ == '__main__':
    analyze(sys.argv[1])
