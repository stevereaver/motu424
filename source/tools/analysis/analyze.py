import pefile
from capstone import *
from capstone.x86 import *
import sys

def analyze_pe(filepath):
    pe = pefile.PE(filepath)
    md = Cs(CS_ARCH_X86, CS_MODE_64 if pe.FILE_HEADER.Machine == pefile.MACHINE_TYPE['IMAGE_FILE_MACHINE_AMD64'] else CS_MODE_32)
    md.detail = True
    
    deviceio_addr = None
    if hasattr(pe, 'DIRECTORY_ENTRY_IMPORT'):
        for entry in pe.DIRECTORY_ENTRY_IMPORT:
            for imp in entry.imports:
                if imp.name and b'DeviceIoControl' in imp.name:
                    # In 32-bit PE, imp.address is the IAT address
                    deviceio_addr = imp.address
                    print(f"Found DeviceIoControl import IAT at 0x{deviceio_addr:x}")

    if not deviceio_addr:
        print("DeviceIoControl not found in imports.")
        return

    for section in pe.sections:
        if b'.text' in section.Name:
            code = section.get_data()
            base_addr = pe.OPTIONAL_HEADER.ImageBase + section.VirtualAddress
            
            last_rdx = None
            last_push = []
            
            for i in md.disasm(code, base_addr):
                if i.id in (X86_INS_MOV, X86_INS_LEA):
                    if len(i.operands) == 2 and i.operands[0].type == X86_OP_REG:
                        reg_name = i.reg_name(i.operands[0].reg)
                        if reg_name in ('edx', 'rdx', 'dx') and i.operands[1].type == X86_OP_IMM:
                            last_rdx = i.operands[1].imm
                
                if i.id == X86_INS_PUSH and len(i.operands) == 1 and i.operands[0].type == X86_OP_IMM:
                    last_push.append(i.operands[0].imm)
                    if len(last_push) > 10:
                        last_push.pop(0)
                        
                if i.id == X86_INS_CALL:
                    target = None
                    # direct call: call 0x...
                    if i.operands[0].type == X86_OP_IMM:
                        target = i.operands[0].imm
                    # indirect call via memory: call qword ptr [rip + disp] or call dword ptr [addr]
                    elif i.operands[0].type == X86_OP_MEM:
                        if i.operands[0].mem.base == X86_REG_RIP:
                            target = i.address + i.size + i.operands[0].mem.disp
                        elif i.operands[0].mem.disp > 0:
                            # 32-bit direct memory address call, like FF 15 XX XX XX XX
                            target = i.operands[0].mem.disp
                            
                    # Check if target matches our IAT entry or thunk
                    # Sometimes the direct target is a thunk that jumps to the IAT.
                    # We just log all calls that are close to DeviceIoControl or if target == deviceio_addr.
                    if target == deviceio_addr:
                        if pe.FILE_HEADER.Machine == pefile.MACHINE_TYPE['IMAGE_FILE_MACHINE_AMD64']:
                            if last_rdx is not None:
                                print(f"Call to DeviceIoControl at 0x{i.address:x} -> IOCTL: 0x{last_rdx:x} (Func: {(last_rdx >> 2) & 0xfff:x})")
                            else:
                                print(f"Call to DeviceIoControl at 0x{i.address:x} -> IOCTL: UNKNOWN")
                        else:
                            # 32-bit: 8 args pushed. The 7th push is dwIoControlCode.
                            if len(last_push) >= 7:
                                ioctl = last_push[-7]
                                print(f"Call to DeviceIoControl at 0x{i.address:x} -> IOCTL: 0x{ioctl:x} (Func: {(ioctl >> 2) & 0xfff:x})")
                            else:
                                print(f"Call to DeviceIoControl at 0x{i.address:x} -> Recent pushes: {[hex(x) for x in last_push]}")

if __name__ == '__main__':
    analyze_pe(sys.argv[1])
