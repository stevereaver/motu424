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

    # The mapping function FUN_0002efc0 stores:
    # dev_ext + 0x08 = object for BAR0
    # dev_ext + 0x10 = object for BAR1
    # Each object has:
    # object + 0x08 = Mapped Virtual Address (BAR base)
    
    # We want to find:
    # mov regA, [dev_ext + 0x10]
    # mov regB, [regA + 0x08]
    # mov [regB + offset], val  <-- THIS IS THE WRITE!
    
    for section in pe.sections:
        if b'.text' in section.Name:
            code = section.get_data()
            rva_base = section.VirtualAddress
            instructions = list(md.disasm(code, pe.OPTIONAL_HEADER.ImageBase + rva_base))
            
            # Use a dictionary to track which registers hold BAR bases
            # { reg_id: (bar_num, timestamp) }
            bar_regs = {}
            obj_regs = {}
            
            for i, inst in enumerate(instructions):
                # Check for load from dev_ext + 0x08 or 0x10
                if inst.mnemonic == 'mov' and len(inst.operands) == 2:
                    op0 = inst.operands[0]
                    op1 = inst.operands[1]
                    if op0.type == X86_OP_REG and op1.type == X86_OP_MEM:
                        # dev_ext is often in rcx or rdx at start of function
                        # but it could be any register.
                        if op1.mem.disp in (0x08, 0x10):
                             obj_regs[op0.reg] = (op1.mem.disp, i)
                             
                # Check for load from obj + 0x08
                if inst.mnemonic == 'mov' and len(inst.operands) == 2:
                    op0 = inst.operands[0]
                    op1 = inst.operands[1]
                    if op0.type == X86_OP_REG and op1.type == X86_OP_MEM:
                        if op1.mem.base in obj_regs and op1.mem.disp == 0x08:
                            # This register now holds the BAR base!
                            bar_num = 0 if obj_regs[op1.mem.base][0] == 0x08 else 1
                            bar_regs[op0.reg] = (bar_num, i)
                            
                # Check for access using a BAR register
                for op in inst.operands:
                    if op.type == X86_OP_MEM and op.mem.base in bar_regs:
                        # Verify register wasn't overwritten in between
                        bar_num, start_idx = bar_regs[op.mem.base]
                        if i - start_idx < 20: # heuristic window
                            print(f"Found BAR{bar_num} access at 0x{inst.address:x}: {inst.mnemonic} {inst.op_str}")
                            # Print some context
                            for k in range(max(0, i-2), min(len(instructions), i+3)):
                                print(f"  {'->' if k==i else '  '} 0x{instructions[k].address:x}: {instructions[k].mnemonic} {instructions[k].op_str}")

if __name__ == '__main__':
    analyze(sys.argv[1])
