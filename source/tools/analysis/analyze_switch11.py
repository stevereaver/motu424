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

    func_rva = 0x169e0
    func_offset = pe.get_offset_from_rva(func_rva)
    
    print(f"Function RVA: 0x{func_rva:x}, Offset: 0x{func_offset:x}")
    
    # We found `jmp qword ptr [rip + 0xcdfc]` earlier at RVA 0x1779d. Let's find it.
    for i in md.disasm(data[func_offset:func_offset+0x1000], pe.OPTIONAL_HEADER.ImageBase + func_rva):
        if i.id == X86_INS_JMP and i.operands[0].type == X86_OP_MEM and i.operands[0].mem.base == X86_REG_RIP:
             # Wait, in 64-bit PE files, the array is often 32-bit *ImageBase relative* RVAs.
             # BUT if it's "jmp qword ptr", it's an array of 64-bit absolute addresses or 
             # wait, it could just be an indirect jump to a single address?
             # No, if it was an array of absolute 64-bit addresses, we couldn't parse it.
             # Wait, switch tables on MSVC x64 are usually:
             # movsxd rax, dword ptr [rip + table + rcx*4]
             # add rax, base
             # jmp rax
             pass
        if i.id in (X86_INS_MOV, X86_INS_MOVSXD) and len(i.operands) == 2 and i.operands[1].type == X86_OP_MEM and i.operands[1].mem.base == X86_REG_RIP and i.operands[1].mem.index != 0:
             print(f"0x{i.address:x}:\t{i.mnemonic}\t{i.op_str}")
             table_address = i.address + i.size + i.operands[1].mem.disp
             table_rva = table_address - pe.OPTIONAL_HEADER.ImageBase
             table_offset = pe.get_offset_from_rva(table_rva)
             print(f"Table RVA: 0x{table_rva:x}, Offset: 0x{table_offset:x}")
             
             for idx in range(16):
                 entry = struct.unpack('<i', data[table_offset + idx*4 : table_offset + idx*4 + 4])[0]
                 target_rva = pe.OPTIONAL_HEADER.ImageBase + table_rva + entry
                 # Wait, MSVC is base of image:
                 target_rva = pe.OPTIONAL_HEADER.ImageBase + entry
                 # Wait, let's just print the raw value
                 print(f"Entry {idx}: 0x{entry:x}")

if __name__ == '__main__':
    analyze(sys.argv[1])
