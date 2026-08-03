import pefile
from capstone import *
from capstone.x86 import *

pe = pefile.PE("windows-drv/motuaw.sys")
base_addr = pe.OPTIONAL_HEADER.ImageBase
md = Cs(CS_ARCH_X86, CS_MODE_64)
md.detail = True

target_rva = 0x37DD8
target_va = base_addr + target_rva

print("Searching for FW size constants and pointers...")

for section in pe.sections:
    if section.Characteristics & 0x20000000: # Executable
        code = section.get_data()
        sec_addr = base_addr + section.VirtualAddress
        
        for i in md.disasm(code, sec_addr):
            # Check for direct IMM matching the size
            for op in i.operands:
                if op.type == X86_OP_IMM:
                    if op.imm in (0x2B2F8, 0xACBE, 44222, target_va, target_rva):
                        print(f"Match IMM at 0x{i.address:X}: {i.mnemonic} {i.op_str}")
            
            # Check for RIP relative memory accesses pointing to the firmware
            if i.mnemonic == 'lea' or (i.mnemonic.startswith('mov') and len(i.operands) == 2 and i.operands[1].type == X86_OP_MEM):
                for op in i.operands:
                    if op.type == X86_OP_MEM and op.mem.base == X86_REG_RIP:
                        target = i.address + i.size + op.mem.disp
                        if abs(target - target_va) < 0x100:
                            print(f"Match RIP-rel at 0x{i.address:X}: {i.mnemonic} {i.op_str} -> {hex(target)}")

