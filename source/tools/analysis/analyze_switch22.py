import sys
from capstone import *
from capstone.x86 import *
import pefile

def analyze(filepath):
    pe = pefile.PE(filepath)
    md = Cs(CS_ARCH_X86, CS_MODE_64)
    md.detail = True
    
    with open(filepath, 'rb') as f:
        data = f.read()

    # The actual control of the device happens over 0x803? No, wait. 
    # FUN_000275e0 uses param_2. param_2 is calculated as (func_code - 0x800).
    # Then FUN_000275e0 checks:
    # dec edx (param_2 is now param_2 - 1). If 0 (param_2 == 1), jump to LAB_000276fa.
    # So if func_code == 0x801, it goes to LAB_000276fa.
    # dec edx. If 0 (param_2 == 2 -> func_code == 0x802), jump to LAB_0002769c.
    # dec edx. If 0 (param_2 == 3 -> func_code == 0x803), jump to LAB_0002763f.
    
    print("If IOCTL function code == 0x803, it enters LAB_0002763f:")
    print("0x2763f: mov rcx, qword ptr [rcx + 0x54]")
    print("0x27648: mov rax, qword ptr [rcx]")
    print("0x2764b: call qword ptr [rax + 8]")
    
    # What are the virtual functions? Let's analyze the vtable at 0x26030 (the biggest one with 148 entries).
    # It has offset 0x252f0, 0x340f4, etc.
    # We saw multiple vtables. Let's look at `0x26c70` where the object was passed.
    # Wait, the C++ objects are likely created in DriverEntry or AddDevice.
    
    # We can try to statically analyze BAR writes instead of working top-down from the IOCTLs.
    # Let's search the whole binary for instructions that write to Memory-Mapped I/O.
    # MMIO writes are typically: `mov [reg + offset], reg` or `mov dword ptr [reg + offset], imm32`
    # But those are very common.
    
    # In Windows, `WRITE_REGISTER_ULONG` is often used. It's a macro for `*(volatile ULONG *)Register = Value;`.
    # Sometimes it includes a memory barrier or `mov [reg], val`.
    
    # Let's check `FUN_0002efc0` which we saw earlier, which seemed to do some hardware-related stuff.
    func_rva = 0x1efc0
    try:
        func_offset = pe.get_offset_from_rva(func_rva)
        for i in md.disasm(data[func_offset:func_offset+0x100], pe.OPTIONAL_HEADER.ImageBase + func_rva):
            print(f"0x{i.address:x}:\t{i.mnemonic}\t{i.op_str}")
            if i.mnemonic == 'ret': break
    except Exception:
        pass

if __name__ == '__main__':
    analyze(sys.argv[1])
