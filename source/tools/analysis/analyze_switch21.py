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

    # FUN_00027820 determines the IOCTL.
    # The IOCTL determines what gets called. Let's trace it.
    # We saw in FUN_00027820:
    # 0x2784d: shr ecx, 2
    # 0x27850: and ecx, 0xfff  <- extracts the Function Code (let's call it func_code)
    # 0x27856: lea edx, [rcx - 0x800]  <- sets edx = func_code - 0x800. Let's call this `cmd_idx`.
    # 0x2785c: mov rax, qword ptr [rax + 0x30]  <- presumably an object pointer (device extension?)
    # 0x27869: mov r8d, dword ptr [r10 + 0x20]  <- arg to param_3
    # 0x2786d: mov rcx, qword ptr [r10]         <- param_1 (some object)
    # 0x27870: call 0x275e0  (param_1 = obj, param_2 = cmd_idx, param_3 = val, param_4 = something)
    
    # In FUN_000275e0:
    # if param_2 == 1:
    #     call 0x27720
    # elif param_2 == 2:
    #     call 0x27490
    # elif param_2 == 3:
    #     obj2 = obj->0x54
    #     call obj2->vtable[1] (offset +8)
    #     if it returns non-zero:
    #         if param_3 == 6: call 0x16dd0
    #         call 0x1c320
    # elif param_2 == 4:
    #     returns something
    
    # Let's print out the code for `FUN_000278a1` where we saw `cmp ecx, 4`
    # Actually wait. If [rdi + 0x40] == 0, it branches to 0x278a1.
    # At 0x278a1: cmp ecx, 4. (ecx is the RAW func_code here!)
    # If func_code == 4 (which means IOCTL base + 0x10):
    #   manipulates some bits.
    
    print("This means the typical IOCTLs we care about fall into the 0x800+ range.")
    print("The primary dispatch is happening in FUN_000275e0 when param_2 == 3.")
    print("param_2 = func_code - 0x800.")
    print("If param_2 == 3, then func_code == 0x803.")
    print("If func_code == 0x803, it invokes virtual function at offset +8.")

if __name__ == '__main__':
    analyze(sys.argv[1])
