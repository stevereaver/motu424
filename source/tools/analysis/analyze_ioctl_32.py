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

    if not deviceio_addr:
        print("DeviceIoControl not found")
        return

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
                        target = inst.operands[0].mem.disp # 32-bit absolute address

                    if target == deviceio_addr:
                        print(f"Call at 0x{inst.address:x}")
                        
                        # In 32-bit stdcall, DeviceIoControl takes 8 arguments pushed onto the stack.
                        # The 2nd argument (dwIoControlCode) is pushed 7th before the call.
                        pushes = []
                        for k in range(i-1, max(-1, i-25), -1):
                            prev = instructions[k]
                            if prev.id == X86_INS_PUSH:
                                if prev.operands[0].type == X86_OP_IMM:
                                    pushes.append(f"0x{prev.operands[0].imm:08X}")
                                elif prev.operands[0].type == X86_OP_MEM:
                                    pushes.append(f"mem({prev.op_str})")
                                elif prev.operands[0].type == X86_OP_REG:
                                    pushes.append(f"reg({prev.op_str})")
                                else:
                                    pushes.append("unknown")
                                
                                if len(pushes) == 8:
                                    break
                                    
                        if len(pushes) >= 7:
                            ioctl = pushes[6]
                            print(f"  Pushed dwIoControlCode: {ioctl}")
                            if ioctl.startswith("0x"):
                                val = int(ioctl, 16)
                                func_code = (val >> 2) & 0xFFF
                                print(f"  -> Function Code: 0x{func_code:03X}")
                        else:
                            print(f"  Could not find enough pushes. Found: {pushes}")

if __name__ == '__main__':
    analyze(sys.argv[1])
