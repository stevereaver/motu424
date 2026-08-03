import sys
from capstone import *
from capstone.x86 import *
import pefile

def analyze(filepath):
    pe = pefile.PE(filepath)
    md = Cs(CS_ARCH_X86, CS_MODE_64)
    md.detail = True
    
    prologue = bytes.fromhex("48895c2408574881ec80000000418bf8")
    
    with open(filepath, 'rb') as f:
        data = f.read()
        
    offset = data.find(prologue)
    if offset == -1:
        print("Prologue not found!")
        return
        
    print(f"Found function at file offset 0x{offset:x}")
    
    in_switch = False
    for i in md.disasm(data[offset:offset+0x2000], 0x10000 + offset):
        if i.address > 0x10000 + offset + 0x400:
            # We already looked at the first 0x400 bytes, maybe it's further down
            if i.id == X86_INS_JMP and i.operands[0].type == X86_OP_MEM:
                 print(f"0x{i.address:x}:\t{i.mnemonic}\t{i.op_str}")
        
if __name__ == '__main__':
    analyze(sys.argv[1])
