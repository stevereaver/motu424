import pefile
from capstone import *
from capstone.x86 import *

pe = pefile.PE('windows-drv/motuaw.sys')
base_addr = pe.OPTIONAL_HEADER.ImageBase
md = Cs(CS_ARCH_X86, CS_MODE_64)

for section in pe.sections:
    if section.Characteristics & 0x20000000:
        code = section.get_data()
        sec_addr = base_addr + section.VirtualAddress
        for i in md.disasm(code, sec_addr):
            if i.mnemonic.startswith('out'):
                print(f'OUT at {hex(i.address)}: {i.mnemonic} {i.op_str}')
