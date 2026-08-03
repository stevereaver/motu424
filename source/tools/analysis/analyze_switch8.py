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

    # The jump table might be at a different offset in this specific PE.
    # Let's search the `.rdata` or `.text` sections for an array of pointers
    # that point to addresses within `FUN_000275e0`'s range.
    
    image_base = pe.OPTIONAL_HEADER.ImageBase
    func_rva = 0x275e0 - 0x10000
    
    # Range of FUN_000275e0 is roughly func_rva to func_rva + 0x1000
    start_rva = func_rva
    end_rva = func_rva + 0x1000
    
    # We are looking for 32-bit RVAs relative to ImageBase (very common for 64-bit jump tables)
    # So we want values where: start_rva <= val <= end_rva
    
    print(f"Looking for RVAs between 0x{start_rva:x} and 0x{end_rva:x}")
    
    for section in pe.sections:
        if b'.rdata' in section.Name or b'.text' in section.Name:
            offset = section.PointerToRawData
            size = section.SizeOfRawData
            
            for i in range(offset, offset + size - 4, 4):
                val = struct.unpack('<I', data[i:i+4])[0]
                if start_rva <= val <= end_rva:
                    # Found one! Is it a sequence?
                    seq_len = 0
                    while True:
                        next_val = struct.unpack('<I', data[i + seq_len*4 : i + seq_len*4 + 4])[0]
                        if start_rva <= next_val <= end_rva:
                            seq_len += 1
                        else:
                            break
                            
                    if seq_len > 5:
                        print(f"Found jump table with {seq_len} entries at file offset 0x{i:x} (RVA 0x{section.VirtualAddress + (i - offset):x})")
                        for j in range(seq_len):
                            rva = struct.unpack('<I', data[i + j*4 : i + j*4 + 4])[0]
                            target_offset = pe.get_offset_from_rva(rva)
                            print(f"\n--- Case {j} (RVA 0x{rva:x}) ---")
                            try:
                                for inst in md.disasm(data[target_offset:target_offset+0x40], image_base + rva):
                                    print(f"0x{inst.address:x}:\t{inst.mnemonic}\t{inst.op_str}")
                                    if inst.id in (X86_INS_RET, X86_INS_JMP):
                                        break
                            except Exception as e:
                                print(f"Error disassembling case {j}")
                        return

if __name__ == '__main__':
    analyze(sys.argv[1])
