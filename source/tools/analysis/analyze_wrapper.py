import pefile
from capstone import *
from capstone.x86 import *

pe = pefile.PE("windows-app/MOTU/Audio/MOTU PCI Audio Console.exe")
base = pe.OPTIONAL_HEADER.ImageBase
md = Cs(CS_ARCH_X86, CS_MODE_32)
md.detail = True

code = pe.sections[0].get_data()
rva = pe.sections[0].VirtualAddress

for i in md.disasm(code, base + rva):
    if 0x41AADA <= i.address <= 0x41AB40:
        print(f"0x{i.address:X}: {i.mnemonic} {i.op_str}")
