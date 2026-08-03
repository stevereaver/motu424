import pefile
from capstone import *
from capstone.x86 import *

def analyze_ioctl(file_path, start_rva):
    pe = pefile.PE(file_path)
    base_addr = pe.OPTIONAL_HEADER.ImageBase
    
    md = Cs(CS_ARCH_X86, CS_MODE_64)
    md.detail = True
    
    start_addr = base_addr + start_rva
    data = pe.get_data(start_rva, 0x2000)
    
    print(f"Analyzing from RVA {hex(start_rva)} (VA {hex(start_addr)})")
    
    for i in md.disasm(data, start_addr):
        # print(f"0x{i.address:X}:\t{i.mnemonic}\t{i.op_str}")
        if i.mnemonic == 'call':
            # Follow calls to find the real handler
            # If it's a relative call
            if i.operands[0].type == X86_OP_IMM:
                target = i.operands[0].imm
                print(f"Call to 0x{target:X}")
        if i.mnemonic == 'jmp':
            if i.operands[0].type == X86_OP_IMM:
                target = i.operands[0].imm
                print(f"Jmp to 0x{target:X}")

analyze_ioctl("windows-drv/motuaw.sys", 0x24b30)
