import pefile
from capstone import *
from capstone.x86 import *

pe = pefile.PE("windows-drv/motuaw.sys")
base_addr = pe.OPTIONAL_HEADER.ImageBase
md = Cs(CS_ARCH_X86, CS_MODE_64)
md.detail = True

for section in pe.sections:
    if section.Characteristics & 0x20000000:
        code = section.get_data()
        sec_addr = base_addr + section.VirtualAddress
        for i in md.disasm(code, sec_addr):
            # Look for comparisons that check the device model ID.
            # MOTU models are usually 0x1, 0x2, 0x3, 0x4, 0x6, 0x9, etc.
            # We are looking for immediate comparisons matching 0x6 or 0x9, which might map to 24I/O or similar devices.
            if i.mnemonic == 'cmp':
                if len(i.operands) == 2 and i.operands[1].type == X86_OP_IMM:
                    if i.operands[1].imm == 0x01137a: # Vendor ID
                        print(f"Vendor Check at 0x{i.address:X}: {i.mnemonic} {i.op_str}")

