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

    # FUN_00024b30 calls FUN_00027820 or handles DeviceControl.
    # Let's verify what `[rdi + 0x40]` is in FUN_00027820.
    # 0x27834:        mov     rdi, rcx  (param_1 is the IRP)
    # 0x27847:        cmp     byte ptr [rdi + 0x40], sil  (sil is 0)
    # The flag at IRP+0x40 determines if we process it like standard IOCTLs (subtracting 0x800).
    
    # Where does 0x2766d (call 0x16dd0) lead?
    # At 0x27668: cmp edi, 6
    # So if Function == 0x806, it calls 0x16dd0.
    
    # What about other functions? Let's check `FUN_000275e0` branches again.
    
    func_rva = 0x1763f
    func_offset = pe.get_offset_from_rva(func_rva)
    for i in md.disasm(data[func_offset:func_offset+0x50], pe.OPTIONAL_HEADER.ImageBase + func_rva):
        print(f"0x{i.address:x}:\t{i.mnemonic}\t{i.op_str}")

if __name__ == '__main__':
    analyze(sys.argv[1])
