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

    # We found `motu_poke` outputs:
    # Offset 0x00: 0x0009277C
    # Offset 0x04: 0xFFFFFF03
    # Offset 0x08: 0xFFFFFF03
    # Offset 0x0c: 0x0009277C
    # Offset 0x10: 0xFFFFFF03
    # Offset 0x14: 0xFFFFFF03
    # Offset 0x18: 0x0248F000
    # Offset 0x1c: 0x000865DC
    # Let's search the assembly for writes to these offsets or values.
    # Particularly, writes to offset 0x00, 0x18, 0x1c from a mapped base pointer.
    
    # We can search all functions in .text
    for section in pe.sections:
        if b'.text' in section.Name:
            code = section.get_data()
            base_addr = pe.OPTIONAL_HEADER.ImageBase + section.VirtualAddress
            
            for i in md.disasm(code, base_addr):
                # Look for `mov dword ptr [reg + 0x18], reg/imm` or similar
                if i.mnemonic.startswith("mov") and len(i.operands) == 2:
                    op0 = i.operands[0]
                    op1 = i.operands[1]
                    if op0.type == X86_OP_MEM and op0.mem.base != 0 and op0.mem.base != X86_REG_RSP and op0.mem.base != X86_REG_RBP and op0.mem.base != X86_REG_RIP:
                        disp = op0.mem.disp
                        if disp in (0x0, 0x4, 0x8, 0xc, 0x10, 0x14, 0x18, 0x1c, 0x20):
                            if op1.type == X86_OP_IMM:
                                val = op1.imm
                                # Check if it looks like a register write or bitmask
                                if val in (0x0248F000, 0x000865DC, 0xFFFFFF03, 0x0009277C):
                                    print(f"0x{i.address:x}:\t{i.mnemonic}\t{i.op_str} (Exact match!)")
                            
                            # Let's also print any write that modifies bits, maybe using `or` or `and`
                
                if i.mnemonic in ("or", "and") and len(i.operands) == 2:
                    op0 = i.operands[0]
                    if op0.type == X86_OP_MEM and op0.mem.base != 0 and op0.mem.base != X86_REG_RSP and op0.mem.base != X86_REG_RBP and op0.mem.base != X86_REG_RIP:
                        disp = op0.mem.disp
                        if disp in (0x0, 0x4, 0x8, 0xc, 0x10, 0x14, 0x18, 0x1c, 0x20):
                            print(f"0x{i.address:x}:\t{i.mnemonic}\t{i.op_str} (Bitwise manipulation of possible MMIO)")

if __name__ == '__main__':
    analyze(sys.argv[1])
