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

    # We now see FUN_000275e0 is at RVA 0x175e0. Let's disassemble from there.
    # The jump table dispatch was:
    # 0x27668:        cmp     edi, 6
    # 0x2766b:        jne     0x27689
    # 0x2766d:        lea     rcx, [rsp + 0x20]
    # 0x27672:        call    0x16dd0
    # Wait, 0x275e0 has multiple blocks!
    # Let's look at FUN_000275e0 start.
    
    func_rva = 0x175e0
    func_offset = pe.get_offset_from_rva(func_rva)
    
    for i in md.disasm(data[func_offset:func_offset+0x200], pe.OPTIONAL_HEADER.ImageBase + func_rva):
        print(f"0x{i.address:x}:\t{i.mnemonic}\t{i.op_str}")

if __name__ == '__main__':
    analyze(sys.argv[1])
