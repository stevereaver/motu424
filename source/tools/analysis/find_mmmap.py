import pefile
from capstone import *
from capstone.x86 import *

pe = pefile.PE("windows-drv/motuaw.sys")
base_addr = pe.OPTIONAL_HEADER.ImageBase
md = Cs(CS_ARCH_X86, CS_MODE_64)

target_iat = 0

for entry in pe.DIRECTORY_ENTRY_IMPORT:
    for imp in entry.imports:
        if imp.name == b'MmMapIoSpace':
            target_iat = imp.address
            break

print(f"MmMapIoSpace IAT VA: {hex(target_iat)}")

for section in pe.sections:
    if section.Characteristics & 0x20000000:
        code = section.get_data()
        sec_addr = base_addr + section.VirtualAddress
        for i in md.disasm(code, sec_addr):
            if i.mnemonic == 'call' or i.mnemonic == 'jmp':
                # Check for call qword ptr [rip + offset]
                if i.op_str.startswith('qword ptr [rip +'):
                    import re
                    m = re.search(r'0x([0-9a-f]+)', i.op_str)
                    if m:
                        offset = int(m.group(1), 16)
                        target_va = i.address + i.size + offset
                        if target_va == target_iat:
                            print(f"Call to MmMapIoSpace at 0x{i.address:X}")

