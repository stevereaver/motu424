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

    # Pattern: 
    # 1. mov reg, [dev_ext + 0x10]
    # 2. ...
    # 3. mov [reg + offset], val
    
    for section in pe.sections:
        if b'.text' in section.Name:
            code = section.get_data()
            rva_base = section.VirtualAddress
            
            # Sliding window of 20 instructions
            instructions = list(md.disasm(code, pe.OPTIONAL_HEADER.ImageBase + rva_base))
            for i in range(len(instructions) - 10):
                inst = instructions[i]
                if inst.mnemonic == 'mov' and len(inst.operands) == 2:
                    op0 = inst.operands[0]
                    op1 = inst.operands[1]
                    if op0.type == X86_OP_REG and op1.type == X86_OP_MEM:
                        if op1.mem.disp == 0x10:
                            # Load from +0x10 into reg
                            bar1_reg = op0.reg
                            # Check next 10 instructions for uses of bar1_reg as base
                            for j in range(1, 10):
                                next_inst = instructions[i + j]
                                for op in next_inst.operands:
                                    if op.type == X86_OP_MEM and op.mem.base == bar1_reg:
                                        print(f"Found BAR1 access at 0x{next_inst.address:x}: {next_inst.mnemonic} {next_inst.op_str}")
                                        print(f"  (Base loaded at 0x{inst.address:x})")

if __name__ == '__main__':
    analyze(sys.argv[1])
