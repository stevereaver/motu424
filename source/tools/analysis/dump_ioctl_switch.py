import sys
from capstone import *
from capstone.x86 import *
import pefile

pe = pefile.PE("windows-drv/motuaw.sys")
base_addr = pe.OPTIONAL_HEADER.ImageBase
md = Cs(CS_ARCH_X86, CS_MODE_64)
md.detail = True

for section in pe.sections:
    if b'.text' in section.Name:
        code = section.get_data()
        sec_addr = base_addr + section.VirtualAddress
        
        # Searching for SUB or CMP instructions with any immediate values
        for i in md.disasm(code, sec_addr):
            if i.id in (X86_INS_CMP, X86_INS_SUB) and len(i.operands) == 2:
                if i.operands[1].type == X86_OP_IMM:
                    imm = i.operands[1].imm
                    if imm == 0x22200f or imm == 0x803:
                        print(f"Match exactly at 0x{i.address:X}: {i.mnemonic} {i.op_str}")
