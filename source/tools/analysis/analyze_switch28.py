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

    for section in pe.sections:
        if b'.text' in section.Name:
            code = section.get_data()
            rva_base = section.VirtualAddress
            instructions = list(md.disasm(code, pe.OPTIONAL_HEADER.ImageBase + rva_base))
            
            for i in range(len(instructions) - 3):
                # 1. Load object from dev_ext + 0x10
                i1 = instructions[i]
                if i1.mnemonic == 'mov' and len(i1.operands) == 2:
                    if i1.operands[1].type == X86_OP_MEM and i1.operands[1].mem.disp == 0x10:
                        reg_obj = i1.operands[0].reg
                        
                        # 2. Load mmio_ptr from obj + 0x08
                        i2 = instructions[i+1]
                        if i2.mnemonic == 'mov' and len(i2.operands) == 2:
                            if i2.operands[1].type == X86_OP_MEM and i2.operands[1].mem.base == reg_obj and i2.operands[1].mem.disp == 0x08:
                                reg_mmio = i2.operands[0].reg
                                
                                # 3. Access MMIO
                                for k in range(2, 5):
                                    ik = instructions[i + k]
                                    for op in ik.operands:
                                        if op.type == X86_OP_MEM and op.mem.base == reg_mmio:
                                            print(f"Found BAR1 write at 0x{ik.address:x}: {ik.mnemonic} {ik.op_str}")
                                            print(f"  (Offset 0x{op.mem.disp:x})")

if __name__ == '__main__':
    analyze(sys.argv[1])
