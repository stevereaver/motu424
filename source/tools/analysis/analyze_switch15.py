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

    # The jump `0x27480: jmp 0x275e0` is interesting.
    # Where does the actual IOCTL switch happen?
    # Earlier we saw `FUN_00027820` handles DeviceControl. Let's trace it.
    
    # Let's search for "DeviceIoControl" calls from the user app again to get the exact IOCTL code,
    # then trace what `FUN_000275e0` does.
    # We found `IOCTL: 0x65dd38 (Func: 74e)`.
    # Let's write a small script to find any CMP instructions against 0x74e or 0x800+ in the driver.
    
    func_rva = 0x169e0 # start of our block
    func_offset = pe.get_offset_from_rva(func_rva)
    
    for i in md.disasm(data[func_offset:func_offset+0x1000], pe.OPTIONAL_HEADER.ImageBase + func_rva):
        if i.mnemonic == 'cmp':
            if len(i.operands) == 2 and i.operands[1].type == X86_OP_IMM:
                val = i.operands[1].imm
                if 0x700 <= val <= 0x900:
                    print(f"0x{i.address:x}:\t{i.mnemonic}\t{i.op_str} (possible IOCTL code comparison)")
        elif i.mnemonic == 'sub' or i.mnemonic == 'add':
            if len(i.operands) == 2 and i.operands[1].type == X86_OP_IMM:
                val = i.operands[1].imm
                if 0x700 <= val <= 0x900:
                    print(f"0x{i.address:x}:\t{i.mnemonic}\t{i.op_str} (possible base subtraction)")

if __name__ == '__main__':
    analyze(sys.argv[1])
