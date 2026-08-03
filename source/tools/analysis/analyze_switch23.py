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

    # FUN_0002efc0 seems to check sizes `0x400000` (4MB) and `0x800000` (8MB).
    # This exactly matches the size of BAR0 and BAR1!
    # It calls 0x2d930, which calls MmMapIoSpace.
    # So this function maps the BARs!
    
    print("Found BAR mapping logic in FUN_0002efc0:")
    print("If size == 4MB (0x400000), maps BAR0 and stores pointer at rdi+8.")
    print("If size == 8MB (0x800000), maps BAR1 and stores pointer at rdi+0x10.")
    print("Therefore, the 'device extension' object (in rdi) has:")
    print("  +0x08 = Mapped BAR0 (DSP / Audio buffer)")
    print("  +0x10 = Mapped BAR1 (Control Registers)")
    
    # We know that `motu_poke` confirmed that BAR1 is indeed the Control Registers.
    # Let's search the assembly for any access using offset `0x10` from a base pointer 
    # to find where Control Registers are written.
    # We're looking for patterns like:
    # mov rax, [rcx + 0x10]   <-- load mapped BAR1
    # mov dword ptr [rax + register_offset], value  <-- write to register
    
    print("Now looking for register accesses...")
    # Actually, we can use grep on the assembly file for `\[... \+ 0x10\]`
    # and then check nearby instructions for writes.

if __name__ == '__main__':
    analyze(sys.argv[1])
