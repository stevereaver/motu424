import pefile
from capstone import *

pe = pefile.PE("windows-drv/motuaw.sys")
base = pe.OPTIONAL_HEADER.ImageBase
md = Cs(CS_ARCH_X86, CS_MODE_64)

with open("motuaw_init_true.asm", "w") as out:
    for section in pe.sections:
        if section.Name.decode().strip('\x00') == 'init':
            code = section.get_data()
            rva = section.VirtualAddress
            for i in md.disasm(code, base + rva):
                out.write(f"0x{i.address:x}:\t{i.mnemonic}\t{i.op_str}\n")
