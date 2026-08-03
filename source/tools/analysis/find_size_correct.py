import pefile
from capstone import *
from capstone.x86 import *

pe = pefile.PE("windows-drv/motuaw.sys")
base = pe.OPTIONAL_HEADER.ImageBase
md = Cs(CS_ARCH_X86, CS_MODE_64)
md.detail = True

for section in pe.sections:
    if section.Characteristics & 0x20000000:
        code = section.get_data()
        rva = section.VirtualAddress
        for i in md.disasm(code, base + rva):
            for op in i.operands:
                if op.type == X86_OP_IMM:
                    if op.imm in (0x2B2F8, 0xACBE):
                        print(f"[{section.Name.decode()}] Found size at 0x{i.address:X}: {i.mnemonic} {i.op_str}")

