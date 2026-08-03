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
                        # Scan back to find what was put into ECX, since it was pushed
                        for k in range(i-1, max(-1, i-30), -1):
                            prev = instructions[k]
                            if prev.id == X86_INS_MOV and prev.operands[0].type == X86_OP_REG and prev.reg_name(prev.operands[0].reg) == 'ecx':
                                if prev.operands[1].type == X86_OP_IMM:
                                    ioctl = prev.operands[1].imm
                                    func_code = (ioctl >> 2) & 0xFFF
                                    print(f"Found IOCTL: 0x{ioctl:08X} (Func: 0x{func_code:03X})")
                                    break
                                elif prev.operands[1].type == X86_OP_MEM:
                                    print(f"IOCTL loaded from mem at 0x{prev.address:x}: {prev.mnemonic} {prev.op_str}")
                                    break

if __name__ == '__main__':
    analyze(sys.argv[1])
