import sys
import struct
from capstone import *
from capstone.x86 import *
import pefile

def analyze(filepath):
    pe = pefile.PE(filepath)
    md = Cs(CS_ARCH_X86, CS_MODE_64)
    md.detail = True
    
    with open(filepath, 'rb') as f:
        data = f.read()

    func_rva = 0x169e0
    func_offset = pe.get_offset_from_rva(func_rva)
    
    print(f"Function RVA: 0x{func_rva:x}, Offset: 0x{func_offset:x}")
    
    # Just print the disassembly for FUN_000275e0 to manually find the jump target
    for i in md.disasm(data[func_offset:func_offset+0x1000], pe.OPTIONAL_HEADER.ImageBase + func_rva):
        if i.id == X86_INS_JMP:
            print(f"0x{i.address:x}:\t{i.mnemonic}\t{i.op_str}")

if __name__ == '__main__':
    analyze(sys.argv[1])
