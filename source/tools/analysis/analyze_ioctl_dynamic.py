import sys
from capstone import *
from capstone.x86 import *
import pefile

def analyze(filepath):
    pe = pefile.PE(filepath)
    md = Cs(CS_ARCH_X86, CS_MODE_64)
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

    print(f"DeviceIoControl IAT: 0x{deviceio_addr:x}")
    
    for section in pe.sections:
        if b'.text' in section.Name:
            code = section.get_data()
            base_addr = pe.OPTIONAL_HEADER.ImageBase + section.VirtualAddress
            
            # Simple heuristic: scan backward from calls to DeviceIoControl to find EDX/RDX assignments
            instructions = list(md.disasm(code, base_addr))
            for i, inst in enumerate(instructions):
                if inst.id == X86_INS_CALL:
                    target = None
                    if inst.operands[0].type == X86_OP_MEM and inst.operands[0].mem.base == X86_REG_RIP:
                        target = inst.address + inst.size + inst.operands[0].mem.disp
                    if target == deviceio_addr:
                        print(f"Call at 0x{inst.address:x}")
                        # Look backward up to 15 instructions
                        for k in range(i-1, max(-1, i-15), -1):
                            prev = instructions[k]
                            if prev.id in (X86_INS_MOV, X86_INS_LEA):
                                if len(prev.operands) == 2 and prev.operands[0].type == X86_OP_REG:
                                    reg = prev.reg_name(prev.operands[0].reg)
                                    if reg in ('edx', 'rdx'):
                                        if prev.operands[1].type == X86_OP_IMM:
                                            ioctl = prev.operands[1].imm
                                            func_code = (ioctl >> 2) & 0xFFF
                                            print(f"  Found IOCTL: 0x{ioctl:08X} (Func: 0x{func_code:03X}) via {prev.mnemonic} {prev.op_str}")
                                            break
                                        elif prev.operands[1].type == X86_OP_MEM:
                                            print(f"  IOCTL loaded from memory: {prev.mnemonic} {prev.op_str}")
                                            break

if __name__ == '__main__':
    analyze(sys.argv[1])
