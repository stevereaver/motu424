import sys
import pefile
from capstone import *
from capstone.x86 import *

def analyze(filepath):
    pe = pefile.PE(filepath)
    md = Cs(CS_ARCH_X86, CS_MODE_64)
    md.detail = True
    
    base_addr = pe.OPTIONAL_HEADER.ImageBase
    
    for section in pe.sections:
        if b'.text' in section.Name:
            code = section.get_data()
            sec_addr = base_addr + section.VirtualAddress
            
            for i in md.disasm(code, sec_addr):
                if i.mnemonic == 'mov':
                    # Looking for writes to offset 0x18 (DMA Base Register)
                    if len(i.operands) == 2 and i.operands[0].type == X86_OP_MEM and i.operands[0].mem.disp == 0x18:
                        if i.operands[1].type == X86_OP_REG:
                            print(f"0x{i.address:X}: {i.mnemonic} {i.op_str}")

if __name__ == '__main__':
    analyze("windows-drv/motuaw.sys")
