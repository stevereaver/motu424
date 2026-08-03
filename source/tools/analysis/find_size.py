import pefile
from capstone import *
from capstone.x86 import *

pe = pefile.PE("windows-drv/motuaw.sys")
base = pe.OPTIONAL_HEADER.ImageBase
target_size = 0x2B2F8

md = Cs(CS_ARCH_X86, CS_MODE_64)
md.detail = True

for section in pe.sections:
    if b'.text' in section.Name:
        code = section.get_data()
        rva = section.VirtualAddress
        for i in md.disasm(code, base + rva):
            for op in i.operands:
                if op.type == X86_OP_IMM:
                    if op.imm == target_size:
                        print(f"Found IMM size reference at 0x{i.address:X}: {i.mnemonic} {i.op_str}")

