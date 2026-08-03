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

    # We know the function that gets called from IOCTL is FUN_00027820.
    # We found FUN_00027820 in motuaw.sys.asm at `00027820`. 
    # Let's find its file offset.
    func_rva = 0x17820 # 0x27820 - 0x10000
    try:
        func_offset = pe.get_offset_from_rva(func_rva)
        print(f"FUN_00027820 is at RVA 0x{func_rva:x}, Offset 0x{func_offset:x}")
        
        for i in md.disasm(data[func_offset:func_offset+0x200], pe.OPTIONAL_HEADER.ImageBase + func_rva):
            print(f"0x{i.address:x}:\t{i.mnemonic}\t{i.op_str}")
            if i.mnemonic in ("sub", "add", "cmp", "lea") and len(i.operands) == 2 and i.operands[1].type == X86_OP_IMM:
                val = i.operands[1].imm
                if 0x700 <= val <= 0x900 or val == 0x800:
                    print(f"  --> FOUND INTERESTING VALUE: 0x{val:x}")
            if i.mnemonic == 'ret':
                break
    except pefile.PEFormatError as e:
        print(f"Error: {e}")

if __name__ == '__main__':
    analyze(sys.argv[1])
