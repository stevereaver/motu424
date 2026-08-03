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
    
    # In Windows kernel drivers compiled with MSVC, jump tables are typically 
    # implemented as `jmp dword ptr [rip + offset]` relative to ImageBase, OR
    # an indirect jump: `jmp qword ptr [rip + offset]` pointing to an array of 64-bit addresses.
    
    # We saw earlier: `jmp qword ptr [rip + 0xcdfc]`
    # Let's find exactly that instruction and its address.
    for i in md.disasm(data[func_offset:func_offset+0x1000], pe.OPTIONAL_HEADER.ImageBase + func_rva):
        if i.mnemonic == "jmp" and i.op_str.startswith("qword ptr [rip + "):
             table_address = i.address + i.size + i.operands[0].mem.disp
             print(f"0x{i.address:x}:\t{i.mnemonic}\t{i.op_str}")
             
             table_rva = table_address - pe.OPTIONAL_HEADER.ImageBase
             table_offset = pe.get_offset_from_rva(table_rva)
             print(f"Table RVA: 0x{table_rva:x}, Offset: 0x{table_offset:x}")
             
             for idx in range(16):
                 entry = struct.unpack('<Q', data[table_offset + idx*8 : table_offset + idx*8 + 8])[0]
                 target_rva = entry - pe.OPTIONAL_HEADER.ImageBase
                 if 0 < target_rva < 0x200000:
                     print(f"\n--- Case {idx} (target absolute 0x{entry:x}, RVA 0x{target_rva:x}) ---")
                     try:
                         target_offset = pe.get_offset_from_rva(target_rva)
                         for inst in md.disasm(data[target_offset:target_offset+0x40], pe.OPTIONAL_HEADER.ImageBase + target_rva):
                             print(f"0x{inst.address:x}:\t{inst.mnemonic}\t{inst.op_str}")
                             if inst.id in (X86_INS_RET, X86_INS_JMP):
                                 break
                     except Exception as e:
                         print(f"Error {e}")
             break

if __name__ == '__main__':
    analyze(sys.argv[1])
