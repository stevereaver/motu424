import pefile
from capstone import *
from capstone.x86 import *

pe = pefile.PE("windows-drv/motuaw.sys")
base_addr = pe.OPTIONAL_HEADER.ImageBase
md = Cs(CS_ARCH_X86, CS_MODE_64)
md.detail = True

target_rva_str = 0x37F67
target_va_str = base_addr + target_rva_str

for section in pe.sections:
    if section.Characteristics & 0x20000000: # Executable
        code = section.get_data()
        sec_addr = base_addr + section.VirtualAddress
        
        for i in md.disasm(code, sec_addr):
            if i.mnemonic == 'lea' or i.mnemonic == 'mov':
                for op in i.operands:
                    if op.type == X86_OP_MEM and op.mem.base == X86_REG_RIP:
                        target = i.address + i.size + op.mem.disp
                        if abs(target - target_va_str) < 0x100:
                            print(f"Match RIP-rel at 0x{i.address:X}: {i.mnemonic} {i.op_str} -> {hex(target)}")

