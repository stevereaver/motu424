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
    
    # 64-bit switch tables often use movsxd rax, dword ptr [rip + table + rcx*4]; add rax, base; jmp rax
    # or lea rax, [rip + table]; movsxd rcx, dword ptr [rax + rbx*4]; add rcx, base; jmp rcx
    # Let's search for this pattern.
    
    for i in md.disasm(data[func_offset:func_offset+0x1000], pe.OPTIONAL_HEADER.ImageBase + func_rva):
        if i.mnemonic == "movsxd":
            print(f"0x{i.address:x}:\t{i.mnemonic}\t{i.op_str}")
            if i.operands[1].type == X86_OP_MEM and i.operands[1].mem.base == X86_REG_RIP:
                table_address = i.address + i.size + i.operands[1].mem.disp
                table_rva = table_address - pe.OPTIONAL_HEADER.ImageBase
                table_offset = pe.get_offset_from_rva(table_rva)
                print(f"Found table at RVA: 0x{table_rva:x}, Offset: 0x{table_offset:x}")
                
                for idx in range(16):
                    entry = struct.unpack('<i', data[table_offset + idx*4 : table_offset + idx*4 + 4])[0]
                    # the base added is usually the image base.
                    target_rva = pe.OPTIONAL_HEADER.ImageBase + entry
                    target_rva -= pe.OPTIONAL_HEADER.ImageBase # it's just `entry` offset from image base ?
                    # MSVC switch tables are ImageBase + entry
                    target_rva = entry 
                    print(f"\n--- Case {idx} (target RVA 0x{target_rva:x}) ---")
                    if 0 < target_rva < 0x200000:
                        try:
                            target_offset = pe.get_offset_from_rva(target_rva)
                            for inst in md.disasm(data[target_offset:target_offset+0x40], pe.OPTIONAL_HEADER.ImageBase + target_rva):
                                print(f"0x{inst.address:x}:\t{inst.mnemonic}\t{inst.op_str}")
                                if inst.id in (X86_INS_RET, X86_INS_JMP):
                                    break
                        except Exception as e:
                            print(f"Error {e}")

if __name__ == '__main__':
    analyze(sys.argv[1])
