import pefile
from capstone import *
from capstone.x86 import *

pe = pefile.PE("windows-drv/motuaw.sys")
base_addr = pe.OPTIONAL_HEADER.ImageBase
md = Cs(CS_ARCH_X86, CS_MODE_64)

for section in pe.sections:
    if section.Characteristics & 0x20000000:
        code = section.get_data()
        sec_addr = base_addr + section.VirtualAddress
        for i in md.disasm(code, sec_addr):
            # Look for bitbang loop: shr, jnc, set bit
            if i.mnemonic in ('shr', 'sar', 'shl', 'sal'):
                # Check next instructions
                print(f"Shift at 0x{i.address:X}: {i.mnemonic} {i.op_str}")

