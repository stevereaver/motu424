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

    # So the IOCTL switch statement we thought was in FUN_000275e0 is not a standard switch.
    # Look at the beginning:
    # 0x275ed:        mov     edi, r8d     <- This is param_3
    # 0x275f0:        mov     rbx, rcx     <- This is param_1 (the device extension probably)
    # 0x275f3:        dec     edx          <- param_2 is tested
    # 0x275f5:        je      0x276fa      <- if param_2 == 1
    # 0x275fb:        dec     edx
    # 0x275fd:        je      0x2769c      <- if param_2 == 2
    # 0x27603:        dec     edx
    # 0x27605:        je      0x2763f      <- if param_2 == 3
    # 0x27607:        dec     edx
    # 0x27609:        je      0x2761e      <- if param_2 == 4
    # So `FUN_000275e0` takes a simple integer (1, 2, 3, 4) in param_2. It's not the raw IOCTL code.
    
    # Wait, in `FUN_00027820`:
    # 0x27840:        mov     ecx, dword ptr [rax + 0x18]  (Function code or Input length?)
    # 0x27843:        mov     r10, qword ptr [rax + 0x20]
    # 0x27847:        cmp     byte ptr [rdi + 0x40], sil
    # 0x2784b:        je      0x278a1
    # 0x2784d:        shr     ecx, 2      <-- ecx (from IRP parameter) is shifted right by 2
    # 0x27850:        and     ecx, 0xfff  <-- ANDed with 0xfff. This is EXACTLY how you extract the Function Code from a CTL_CODE!
    # 0x27856:        lea     edx, [rcx - 0x800]  <-- The Function code has 0x800 subtracted!
    # 0x2785c:        mov     rax, qword ptr [rax + 0x30]
    # 0x27860:        mov     qword ptr [rsp + 0x20], rax
    # 0x27865:        mov     r9, qword ptr [r10 + 8]
    # 0x27869:        mov     r8d, dword ptr [r10 + 0x20]
    # 0x2786d:        mov     rcx, qword ptr [r10]
    # 0x27870:        call    0x275e0   <-- Calls FUN_000275e0
    
    # So if the IOCTL was 0x65dd38:
    # Function Code = (0x65dd38 >> 2) & 0xFFF = 0x74e
    # param_2 (edx) = 0x74e - 0x800 = -0xb2 ??? That's negative.
    
    # Let's write a small loop to print the function codes that DO NOT go through `FUN_000275e0`.
    # Notice at `0x27847` it jumps to `0x278a1` if `[rdi + 0x40] == 0`.
    # Let's disassemble `0x278a1` onwards.
    
    func_rva = 0x178a1
    func_offset = pe.get_offset_from_rva(func_rva)
    for i in md.disasm(data[func_offset:func_offset+0x100], pe.OPTIONAL_HEADER.ImageBase + func_rva):
        print(f"0x{i.address:x}:\t{i.mnemonic}\t{i.op_str}")
        if i.mnemonic == 'ret': break

if __name__ == '__main__':
    analyze(sys.argv[1])
