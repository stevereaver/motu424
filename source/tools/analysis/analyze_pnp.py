import pefile
from capstone import *

pe = pefile.PE("windows-drv/motuaw.sys")
base = pe.OPTIONAL_HEADER.ImageBase
md = Cs(CS_ARCH_X86, CS_MODE_64)

def dump_func(va, name, length=0x400):
    print(f"--- {name} at {hex(va)} ---")
    rva = va - base
    
    # find section
    for s in pe.sections:
        if s.VirtualAddress <= rva < s.VirtualAddress + s.Misc_VirtualSize:
            offset = rva - s.VirtualAddress
            code = s.get_data()[offset:offset+length]
            for i in md.disasm(code, va):
                print(f"0x{i.address:x}:\t{i.mnemonic}\t{i.op_str}")
            return

dump_func(0x275e0, "IOCTL Switch Statement", 0x1000)

