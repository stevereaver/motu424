import sys
import pefile
from capstone import *
from capstone.x86 import *

def analyze(filepath):
    pe = pefile.PE(filepath)
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    md.detail = True
    
    wrapper_addr = 0x417C90
    base_addr = pe.OPTIONAL_HEADER.ImageBase
    
    for section in pe.sections:
        if b'.text' in section.Name:
            code = section.get_data()
            sec_addr = base_addr + section.VirtualAddress
            
            insns = list(md.disasm(code, sec_addr))
            for i, inst in enumerate(insns):
                if inst.id == X86_INS_CALL:
                    target = None
                    if inst.operands[0].type == X86_OP_IMM:
                        target = inst.operands[0].imm
                        
                    if target == wrapper_addr:
                        # Find the 4th push before this call
                        pushes = []
                        for j in range(i-1, max(-1, i-20), -1):
                            prev = insns[j]
                            if prev.id == X86_INS_PUSH:
                                if prev.operands[0].type == X86_OP_IMM:
                                    pushes.append(prev.operands[0].imm)
                                else:
                                    pushes.append(None)
                            if len(pushes) == 4:
                                break
                        
                        if len(pushes) == 4:
                            func_id = pushes[3]
                            if func_id is not None:
                                print(f"Call at 0x{inst.address:X}: func_id = {func_id} (IOCTL = 0x222{func_id*4:03X}3, FuncCode = 0x{0x800 + func_id:X})")
                            else:
                                print(f"Call at 0x{inst.address:X}: func_id is dynamic")

if __name__ == '__main__':
    analyze(sys.argv[1])