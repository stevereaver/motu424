import pefile
from capstone import *
from capstone.x86 import *

pe = pefile.PE("windows-drv/motuaw.sys")
base = pe.OPTIONAL_HEADER.ImageBase
target_addr = base + 0x37DD8
target_size = 0x2B2F8

md = Cs(CS_ARCH_X86, CS_MODE_64)
md.detail = True

for section in pe.sections:
    if section.Characteristics & 0x20000000: # Executable
        code = section.get_data()
        rva = section.VirtualAddress
        for i in md.disasm(code, base + rva):
            for op in i.operands:
                if op.type == X86_OP_MEM:
                    if op.mem.base == X86_REG_RIP:
                        ref_addr = i.address + i.size + op.mem.disp
                        if ref_addr == target_addr:
                            print(f"[{section.Name.decode()}] Found RIP reference at 0x{i.address:X}: {i.mnemonic} {i.op_str}")
                elif op.type == X86_OP_IMM:
                    if op.imm == target_size:
                        print(f"[{section.Name.decode()}] Found size 0x{target_size:X} at 0x{i.address:X}: {i.mnemonic} {i.op_str}")
