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

    print(f"DeviceIoControl IAT: 0x{deviceio_addr:x}")

    for section in pe.sections:
        if b'.text' in section.Name:
            code = section.get_data()
            base_addr = pe.OPTIONAL_HEADER.ImageBase + section.VirtualAddress
            
            instructions = list(md.disasm(code, base_addr))
            for i, inst in enumerate(instructions):
                if inst.id == X86_INS_CALL:
                    target = None
                    if inst.operands[0].type == X86_OP_MEM and inst.operands[0].mem.disp > 0:
                        target = inst.operands[0].mem.disp
                    if target == deviceio_addr:
                        print(f"Call at 0x{inst.address:x}")
                        pushes = []
                        for k in range(i-1, max(-1, i-25), -1):
                            prev = instructions[k]
                            if prev.id == X86_INS_PUSH:
                                if prev.operands[0].type == X86_OP_IMM:
                                    pushes.append(f"0x{prev.operands[0].imm:08X}")
                                else:
                                    pushes.append(f"reg/mem({prev.op_str})")
                                if len(pushes) == 8: break
                        if len(pushes) >= 7:
                            ioctl = pushes[6]
                            print(f"  Pushed dwIoControlCode: {ioctl}")
                            if ioctl.startswith("0x"):
                                val = int(ioctl, 16)
                                print(f"  -> Function Code: 0x{(val >> 2) & 0xFFF:03X}")

if __name__ == '__main__':
    analyze(sys.argv[1])
