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
                        # 0x41ab2c was where the load happened, let's trace back from the start of the function
                        # Look for 'push ebp; mov ebp, esp'
                        func_start = 0
                        for j in range(i, -1, -1):
                            if instructions[j].mnemonic == 'push' and instructions[j].op_str == 'ebp':
                                func_start = j
                                break
                        
                        print(f"Function starts at 0x{instructions[func_start].address:x}")
                        for j in range(func_start, i+1):
                            ins = instructions[j]
                            if ins.mnemonic == 'mov' and 'ebp - 4' in ins.op_str:
                                print(f"  0x{ins.address:x}: {ins.mnemonic} {ins.op_str}")

if __name__ == '__main__':
    analyze(sys.argv[1])
