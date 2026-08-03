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

    image_base = pe.OPTIONAL_HEADER.ImageBase
    
    # 0x26c9f:        jmp     0x26cf8 -> Wait, earlier jump was:
    # 0x2779d:        jmp     qword ptr [rip + 0xcdfc]
    # The jump is NOT a table of 32-bit RVAs! It's an indirect jump.
    # jmp qword ptr [rip + 0xcdfc] at 0x2779d.
    # Instruction is 7 bytes: FF 25 FC CD 00 00
    # Next RIP is 0x277a4
    # Address is 0x277a4 + 0xcdfc = 0x345a0
    
    table_rva = 0x345a0
    table_offset = pe.get_offset_from_rva(table_rva)
    
    print(f"Table offset: 0x{table_offset:x}")
    if table_offset != -1:
        # It's an array of 64-bit absolute addresses or RVAs? "qword ptr" means 64-bit addresses.
        for i in range(15):
             entry_bytes = data[table_offset + i*8 : table_offset + i*8 + 8]
             abs_addr = struct.unpack('<Q', entry_bytes)[0]
             
             # Convert absolute address to RVA
             rva = abs_addr - image_base
             
             try:
                 target_offset = pe.get_offset_from_rva(rva)
                 print(f"\n--- Case {i} (target absolute 0x{abs_addr:x}, RVA 0x{rva:x}) ---")
                 for inst in md.disasm(data[target_offset:target_offset+0x40], abs_addr):
                     print(f"0x{inst.address:x}:\t{inst.mnemonic}\t{inst.op_str}")
                     if inst.id in (X86_INS_RET, X86_INS_JMP):
                         break
             except Exception as e:
                 print(f"Case {i}: Invalid Absolute Address 0x{abs_addr:x} (RVA: 0x{rva:x})")

if __name__ == '__main__':
    analyze(sys.argv[1])
