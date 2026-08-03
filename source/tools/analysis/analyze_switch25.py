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

    # Offset 0x10 in device extension is BAR1 (Registers)
    # Offset 0x08 in device extension is BAR0 (DSP/Buffers)
    
    for section in pe.sections:
        if b'.text' in section.Name:
            code = section.get_data()
            base_addr = pe.OPTIONAL_HEADER.ImageBase + section.VirtualAddress
            
            for i in md.disasm(code, base_addr):
                # Pattern: mov reg, [reg_base + 0x10]
                if i.mnemonic == 'mov' and len(i.operands) == 2:
                    op0 = i.operands[0]
                    op1 = i.operands[1]
                    if op0.type == X86_OP_REG and op1.type == X86_OP_MEM:
                        if op1.mem.disp == 0x10:
                             # Possible BAR1 load. Let's look ahead for uses of op0.reg
                             reg_bar1 = op0.reg
                             # Scan next 10 instructions
                             pass 

    # Simpler: just search for all [reg + disp] where disp is small and reg is NOT RSP/RBP/RIP
    # and it is a write or read.
    print("Found potential register accesses:")
    for section in pe.sections:
        if b'.text' in section.Name:
            code = section.get_data()
            base_addr = pe.OPTIONAL_HEADER.ImageBase + section.VirtualAddress
            for i in md.disasm(code, base_addr):
                for op in i.operands:
                    if op.type == X86_OP_MEM:
                        if op.mem.base not in (0, X86_REG_RSP, X86_REG_RBP, X86_REG_RIP):
                            if 0 <= op.mem.disp <= 0x100:
                                # Found a small offset access.
                                # Check if the instruction is a move to/from this memory.
                                if i.mnemonic in ('mov', 'movzx', 'movsxd', 'or', 'and', 'xor'):
                                    # To reduce noise, only print if we see consecutive accesses to the same base
                                    pass

    # Actually, let's just look at FUN_00016dd0 and FUN_0001c320 which we saw earlier.
    # Those were called when IOCTL 0x803 was used.
    for func_rva_hex in ('0x6dd0', '0xc320', '0x272d0', '0x27720'):
        rva = int(func_rva_hex, 16)
        try:
            off = pe.get_offset_from_rva(rva)
            print(f"\n--- Disassembling {func_rva_hex} ---")
            for i in md.disasm(data[off:off+0x100], pe.OPTIONAL_HEADER.ImageBase + rva):
                print(f"0x{i.address:x}:\t{i.mnemonic}\t{i.op_str}")
                if i.mnemonic == 'ret': break
        except: pass

if __name__ == '__main__':
    analyze(sys.argv[1])
