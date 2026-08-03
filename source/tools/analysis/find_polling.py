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
        
        # We look for a pattern:
        # mov reg, [mem]  (mem is often offset 0, 0x18, 0x1C, 0x20)
        # test reg, imm / and reg, imm
        # jz / jnz back
        
        insns = list(md.disasm(code, base + rva))
        for i in range(len(insns) - 2):
            inst1 = insns[i]
            if inst1.mnemonic in ('mov', 'movzx'):
                if len(inst1.operands) == 2 and inst1.operands[1].type == X86_OP_MEM:
                    disp = inst1.operands[1].mem.disp
                    if disp in (0, 0x04, 0x08, 0x0C, 0x10, 0x14, 0x18, 0x1C, 0x20):
                        inst2 = insns[i+1]
                        if inst2.mnemonic in ('test', 'and', 'cmp'):
                            inst3 = insns[i+2]
                            if inst3.mnemonic in ('je', 'jz', 'jne', 'jnz'):
                                # Check if the jump target is backwards
                                if inst3.operands[0].type == X86_OP_IMM:
                                    target = inst3.operands[0].imm
                                    if target <= inst3.address:
                                        print(f"[{section.Name.decode()}] Polling loop at 0x{inst1.address:X}: read from disp 0x{disp:X}")
                                        print(f"  {inst1.mnemonic} {inst1.op_str}")
                                        print(f"  {inst2.mnemonic} {inst2.op_str}")
                                        print(f"  {inst3.mnemonic} {inst3.op_str}")

