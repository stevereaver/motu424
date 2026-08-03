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
            if i.mnemonic in ('or', 'and', 'xor'):
                if len(i.operands) == 2 and i.operands[0].type == X86_OP_MEM and i.operands[1].type == X86_OP_IMM:
                    disp = i.operands[0].mem.disp
                    if disp in (0, 0x18, 0x1c, 0x20, 0x04, 0x08, 0x10, 0x14):
                        val = i.operands[1].imm
                        print(f"[{section.Name.decode()}] 0x{i.address:X}: {i.mnemonic} {i.op_str}")

