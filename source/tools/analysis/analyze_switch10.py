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

    # Rather than guessing the jump table address based on Ghidra's disassembly,
    # let's find the exact instruction sequence in the PE file for `FUN_000275e0`
    
    # Assembly from earlier:
    # 0x26c60:        mov     qword ptr [rsp + 0x20], rax
    # 0x26c65:        mov     r9, qword ptr [r10 + 8]
    # 0x26c69:        mov     r8d, dword ptr [r10 + 0x20]
    # 0x26c6d:        mov     rcx, qword ptr [r10]
    # 0x26c70:        call    0x269e0  (this is call to FUN_000275e0)
    
    # We found `FUN_000275e0` at RVA 0x169e0 in earlier script (file offset 0x169e0 in our fake base but RVA is what matters)
    func_rva = 0x169e0
    func_offset = pe.get_offset_from_rva(func_rva)
    
    print(f"Function RVA: 0x{func_rva:x}, Offset: 0x{func_offset:x}")
    
    in_switch = False
    for i in md.disasm(data[func_offset:func_offset+0x1000], pe.OPTIONAL_HEADER.ImageBase + func_rva):
        if i.id == X86_INS_JMP and i.operands[0].type == X86_OP_MEM and i.operands[0].mem.base == X86_REG_RIP:
             table_address = i.address + i.size + i.operands[0].mem.disp
             print(f"0x{i.address:x}:\t{i.mnemonic}\t{i.op_str}")
             print(f"Found jump table address: 0x{table_address:x}")
             
             table_rva = table_address - pe.OPTIONAL_HEADER.ImageBase
             table_offset = pe.get_offset_from_rva(table_rva)
             print(f"Table RVA: 0x{table_rva:x}, Offset: 0x{table_offset:x}")
             
             # MSVC x64 switch tables are arrays of 32-bit *ImageBase relative* RVAs.
             for idx in range(16):
                 entry = struct.unpack('<I', data[table_offset + idx*4 : table_offset + idx*4 + 4])[0]
                 target_rva = entry
                 print(f"\n--- Case {idx} (target RVA 0x{target_rva:x}) ---")
                 try:
                     target_offset = pe.get_offset_from_rva(target_rva)
                     for inst in md.disasm(data[target_offset:target_offset+0x40], pe.OPTIONAL_HEADER.ImageBase + target_rva):
                         print(f"0x{inst.address:x}:\t{inst.mnemonic}\t{inst.op_str}")
                         if inst.id in (X86_INS_RET, X86_INS_JMP):
                             break
                 except Exception as e:
                     print(f"Error reading target {e}")
             break

if __name__ == '__main__':
    analyze(sys.argv[1])
