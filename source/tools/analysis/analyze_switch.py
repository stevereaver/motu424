import sys
from capstone import *
from capstone.x86 import *
import pefile

def analyze(filepath):
    pe = pefile.PE(filepath)
    md = Cs(CS_ARCH_X86, CS_MODE_64)
    md.detail = True
    
    # In Ghidra, the function is at 000275e0 relative to the image base.
    # The actual RVA depends on the image base from Ghidra which is usually 0x10000 for sys.
    # Let's just find the file offset directly using pefile's translation.
    
    target_rva = 0x275e0 - 0x10000 # Adjusting for PE file base if Ghidra assumed 0x10000
    
    # Just to be safe, search the file content for the prologue bytes:
    # 48 89 5c 24 08 57 48 81 ec 80 00 00 00 41 8b f8
    prologue = bytes.fromhex("48895c2408574881ec80000000418bf8")
    
    with open(filepath, 'rb') as f:
        data = f.read()
        
    offset = data.find(prologue)
    if offset == -1:
        print("Prologue not found!")
        return
        
    print(f"Found function at file offset 0x{offset:x}")
    
    for i in md.disasm(data[offset:offset+0x400], 0x10000 + offset): # Fake base
        print(f"0x{i.address:x}:\t{i.mnemonic}\t{i.op_str}")

if __name__ == '__main__':
    analyze(sys.argv[1])
