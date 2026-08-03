import sys
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
    
    print("Searching for bitwise OR/AND operations on memory...")
    
    for section in pe.sections:
        if b'.text' in section.Name:
            code = section.get_data()
            rva_base = section.VirtualAddress
            
            for i in md.disasm(code, image_base + rva_base):
                if i.mnemonic in ('or', 'and', 'xor'):
                    if len(i.operands) == 2 and i.operands[0].type == X86_OP_MEM and i.operands[1].type == X86_OP_IMM:
                        disp = i.operands[0].mem.disp
                        if disp in (0, 0x18, 0x1c, 0x20):
                            val = i.operands[1].imm
                            # Common toggle bits
                            if val in (1, 2, 4, 8, 0x10, 0x20, 0x40, 0x80, 0x100, 0x200) or val >= 0xfffffff0:
                                print(f"0x{i.address:x}: {i.mnemonic} {i.op_str}")
                # Sometimes it reads to reg, modifies reg, writes back
                elif i.mnemonic == 'mov' and len(i.operands) == 2:
                    if i.operands[0].type == X86_OP_REG and i.operands[1].type == X86_OP_MEM:
                        disp = i.operands[1].mem.disp
                        # if disp == 0: ... too noisy, skip for now.

if __name__ == '__main__':
    analyze(sys.argv[1])
