import sys
from capstone import *
from capstone.x86 import *
import pefile

def analyze(filepath):
    pe = pefile.PE(filepath)
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    md.detail = True
    
    addr = 0x40b0e0
    
    for section in pe.sections:
        if b'.text' in section.Name:
            code = section.get_data()
            base_addr = pe.OPTIONAL_HEADER.ImageBase + section.VirtualAddress
            
            if base_addr <= addr < base_addr + section.SizeOfRawData:
                offset = addr - base_addr
                for i in md.disasm(code[offset:offset+0x40], addr):
                    print(f"0x{i.address:x}: {i.mnemonic} {i.op_str}")
                    if i.mnemonic == 'ret': break

if __name__ == '__main__':
    analyze(sys.argv[1])
