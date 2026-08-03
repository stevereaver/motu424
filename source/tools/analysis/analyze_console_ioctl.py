import sys
from capstone import *
from capstone.x86 import *
import pefile

def analyze(filepath):
    pe = pefile.PE(filepath)
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    md.detail = True
    
    deviceio_addr = None
    if hasattr(pe, 'DIRECTORY_ENTRY_IMPORT'):
        for entry in pe.DIRECTORY_ENTRY_IMPORT:
            for imp in entry.imports:
                if imp.name and b'DeviceIoControl' in imp.name:
                    deviceio_addr = imp.address

    if not deviceio_addr: return
    
    print(f"DeviceIoControl at 0x{deviceio_addr:x}")

    for section in pe.sections:
        if b'.text' in section.Name:
            code = section.get_data()
            base_addr = pe.OPTIONAL_HEADER.ImageBase + section.VirtualAddress
            
            insns = list(md.disasm(code, base_addr))
            for i, inst in enumerate(insns):
                if inst.id == X86_INS_CALL:
                    target = None
                    if inst.operands[0].type == X86_OP_MEM and inst.operands[0].mem.disp == deviceio_addr:
                        target = deviceio_addr
                        
                    if target == deviceio_addr:
                        # Find the 7th push before this call
                        pushes = []
                        for j in range(i-1, max(-1, i-30), -1):
                            prev = insns[j]
                            if prev.id == X86_INS_PUSH:
                                if prev.operands[0].type == X86_OP_IMM:
                                    pushes.append(prev.operands[0].imm)
                                else:
                                    pushes.append(None) # Not an immediate
                            if len(pushes) == 8:
                                break
                        
                        if len(pushes) >= 7:
                            ioctl = pushes[6]
                            if ioctl is not None:
                                func_code = (ioctl >> 2) & 0xFFF
                                print(f"Call at 0x{inst.address:X}: IOCTL = 0x{ioctl:08X} (Func = 0x{func_code:03X} / {func_code})")
                            else:
                                # Sometimes it's passed via register
                                print(f"Call at 0x{inst.address:X}: IOCTL is dynamic")

if __name__ == '__main__':
    analyze(sys.argv[1])
