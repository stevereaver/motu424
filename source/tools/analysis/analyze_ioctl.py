import sys
from capstone import *
from capstone.x86 import *
import pefile

def analyze(filepath):
    pe = pefile.PE(filepath)
    md = Cs(CS_ARCH_X86, CS_MODE_64 if pe.FILE_HEADER.Machine == pefile.MACHINE_TYPE['IMAGE_FILE_MACHINE_AMD64'] else CS_MODE_32)
    md.detail = True
    
    deviceio_addr = None
    if hasattr(pe, 'DIRECTORY_ENTRY_IMPORT'):
        for entry in pe.DIRECTORY_ENTRY_IMPORT:
            for imp in entry.imports:
                if imp.name and b'DeviceIoControl' in imp.name:
                    deviceio_addr = imp.address

    if not deviceio_addr:
        print("DeviceIoControl not found")
        return

    print(f"Analyzing calls to DeviceIoControl (IAT: 0x{deviceio_addr:x})")
    
    ioctls = set()
    for section in pe.sections:
        if b'.text' in section.Name:
            code = section.get_data()
            base_addr = pe.OPTIONAL_HEADER.ImageBase + section.VirtualAddress
            
            last_movs = {}
            last_pushes = []
            
            for i in md.disasm(code, base_addr):
                if i.id in (X86_INS_MOV, X86_INS_LEA):
                    if len(i.operands) == 2 and i.operands[0].type == X86_OP_REG and i.operands[1].type == X86_OP_IMM:
                        reg_name = i.reg_name(i.operands[0].reg)
                        last_movs[reg_name] = i.operands[1].imm
                
                if i.id == X86_INS_PUSH and len(i.operands) == 1 and i.operands[0].type == X86_OP_IMM:
                    last_pushes.append(i.operands[0].imm)
                    if len(last_pushes) > 15: last_pushes.pop(0)

                if i.id == X86_INS_CALL:
                    target = None
                    if i.operands[0].type == X86_OP_IMM:
                        target = i.operands[0].imm
                    elif i.operands[0].type == X86_OP_MEM:
                        if i.operands[0].mem.base == X86_REG_RIP:
                            target = i.address + i.size + i.operands[0].mem.disp
                        elif i.operands[0].mem.disp > 0:
                            target = i.operands[0].mem.disp

                    if target == deviceio_addr:
                        if pe.FILE_HEADER.Machine == pefile.MACHINE_TYPE['IMAGE_FILE_MACHINE_AMD64']:
                            # In x64, dwIoControlCode is the 2nd argument (RDX/EDX)
                            ioctl = last_movs.get('rdx') or last_movs.get('edx')
                            if ioctl:
                                ioctls.add(ioctl)
                        else:
                            # In x86 stdcall, dwIoControlCode is pushed 7th before call (if 8 args)
                            if len(last_pushes) >= 7:
                                ioctls.add(last_pushes[-7])

    print("Discovered IOCTLs:")
    for ioctl in sorted(ioctls):
        func_code = (ioctl >> 2) & 0xFFF
        print(f"  0x{ioctl:08X} (Function Code: 0x{func_code:03X} / {func_code})")

if __name__ == '__main__':
    analyze(sys.argv[1])
