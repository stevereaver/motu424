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

    # FUN_000275e0 is a simple 1 to 4 switch. 
    # Wait, earlier I said "FUN_000275e0 is the giant switch table for the card".
    # I got confused. `0x26c70: call 0x269e0` (which is FUN_000275e0).
    # FUN_000275e0 has this branch:
    # 0x2764b: call qword ptr [rax + 8] -> This is a virtual function call!
    # Because `rax` comes from `qword ptr [rcx]` which is a vtable pointer.
    
    # Let's search the `.rdata` section for vtables that have functions matching our IOCTL logic.
    # A vtable will be a list of function pointers.
    
    image_base = pe.OPTIONAL_HEADER.ImageBase
    print(f"Image Base: 0x{image_base:x}")
    
    # We saw "PCI424Driver.cpp" references. Let's see if we can find its vtable.
    # We can just dump all function addresses in the .rdata section that look like vtables.
    
    for section in pe.sections:
        if b'.rdata' in section.Name:
            rdata_data = section.get_data()
            rdata_rva = section.VirtualAddress
            
            # Look for consecutive pointers that point to the .text section
            import struct
            text_section = None
            for s in pe.sections:
                if b'.text' in s.Name:
                    text_section = s
                    break
                    
            if not text_section: return
            
            text_start = image_base + text_section.VirtualAddress
            text_end = text_start + text_section.SizeOfRawData
            
            consecutive = 0
            start_rva = 0
            
            for i in range(0, len(rdata_data), 8):
                if i + 8 > len(rdata_data): break
                ptr = struct.unpack('<Q', rdata_data[i:i+8])[0]
                if text_start <= ptr < text_end:
                    if consecutive == 0:
                        start_rva = rdata_rva + i
                    consecutive += 1
                else:
                    if consecutive > 5:
                        print(f"Possible vtable at RVA 0x{start_rva:x} with {consecutive} entries")
                        for j in range(consecutive):
                            func_ptr = struct.unpack('<Q', rdata_data[start_rva - rdata_rva + j*8 : start_rva - rdata_rva + j*8 + 8])[0]
                            print(f"  vfunc {j}: 0x{func_ptr:x}")
                    consecutive = 0

if __name__ == '__main__':
    analyze(sys.argv[1])
