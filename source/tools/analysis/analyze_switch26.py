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

    image_base = pe.OPTIONAL_HEADER.ImageBase
    
    # We are looking for:
    # 1. mov regX, [regY + 0x10]  (load BAR1 base)
    # 2. mov [regX + offset], val (write to BAR1)
    
    for section in pe.sections:
        if b'.text' in section.Name:
            code = section.get_data()
            rva_base = section.VirtualAddress
            
            # Use a sliding window to find the pattern
            # For efficiency, let's just find all loads from +0x10
            for i in md.disasm(code, image_base + rva_base):
                if i.mnemonic == 'mov' and len(i.operands) == 2:
                    op0 = i.operands[0]
                    op1 = i.operands[1]
                    if op0.type == X86_OP_REG and op1.type == X86_OP_MEM:
                        if op1.mem.disp == 0x10:
                            # Possible BAR1 load into op0.reg
                            reg_name = i.reg_name(op0.reg)
                            # Look at next few instructions for uses of this register
                            # (This is a bit slow but let's try it for a small range or limited results)
                            pass
                            
    # Let's try a different approach. Search for any instruction that writes to a register
    # and has a displacement that looks like a MOTU register.
    # MOTU 424 usually has registers at 0x08, 0x0C, 0x10, etc.
    
    print("Searching for writes to BAR1 offsets...")
    for section in pe.sections:
        if b'.text' in section.Name:
            code = section.get_data()
            rva_base = section.VirtualAddress
            for i in md.disasm(code, image_base + rva_base):
                if i.mnemonic in ('mov', 'or', 'and', 'xor'):
                    if len(i.operands) == 2 and i.operands[0].type == X86_OP_MEM:
                        mem = i.operands[0].mem
                        # Exclude stack and rip-relative
                        if mem.base not in (0, X86_REG_RSP, X86_REG_RBP, X86_REG_RIP):
                            # Check if the base register was recently loaded from dev_ext + 0x10
                            # This is hard without full data flow.
                            # Let's just print any instruction that accesses offset 0x08 or 0x0C or 0x18
                            # in a register-relative way.
                            if mem.disp in (0x0, 0x08, 0x0c, 0x18, 0x1c, 0x20):
                                # To keep it clean, only print if it's in the main driver logic
                                if 0x20000 <= i.address <= 0x35000:
                                    print(f"0x{i.address:x}:\t{i.mnemonic}\t{i.op_str}")

if __name__ == '__main__':
    analyze(sys.argv[1])
