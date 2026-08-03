import sys
import struct
from capstone import *
from capstone.x86 import *
import pefile

def analyze(filepath):
    pe = pefile.PE(filepath)
    md = Cs(CS_ARCH_X86, CS_MODE_64)
    md.detail = True
    
    with open(filepath, 'rb') as f:
        data = f.read()

    # The function is FUN_000275e0. In ghidra it was 0x275e0 (ImageBase=0x10000 -> RVA 0x175e0)
    # The jump was at 0x2779d -> RVA 0x1779d
    # Instruction is 7 bytes: jmp qword ptr [rip + 0xcdfc]
    # RIP after instruction: 0x1779d + 7 = 0x177a4
    # Address relative to rip = 0x177a4 + 0xcdfc = 0x245a0
    # Let's read 0x245a0 RVA as a sequence of QWORDS (since it's a 64-bit jump table)
    
    table_rva = 0x245a0
    try:
        table_offset = pe.get_offset_from_rva(table_rva)
    except pefile.PEFormatError:
        print("Cannot find RVA")
        return
        
    print(f"Jump table RVA: 0x{table_rva:x}, offset: 0x{table_offset:x}")
    image_base = pe.OPTIONAL_HEADER.ImageBase
    
    for i in range(16):
        entry_bytes = data[table_offset + i*8 : table_offset + (i+1)*8]
        abs_addr = struct.unpack('<Q', entry_bytes)[0]
        
        # In a PE file on disk, an absolute pointer might be stored as an ImageBase + RVA.
        # But wait, relocation applies when loaded. If it's a QWORD in .rdata, it's just ImageBase + RVA
        rva = abs_addr - image_base
        print(f"\n--- Case {i} (target absolute 0x{abs_addr:x}, RVA 0x{rva:x}) ---")
        try:
            target_offset = pe.get_offset_from_rva(rva)
            for inst in md.disasm(data[target_offset:target_offset+0x40], abs_addr):
                print(f"0x{inst.address:x}:\t{inst.mnemonic}\t{inst.op_str}")
                if inst.id in (X86_INS_RET, X86_INS_JMP):
                    break
        except Exception as e:
            print(f"Error: {e}")

if __name__ == '__main__':
    analyze(sys.argv[1])
