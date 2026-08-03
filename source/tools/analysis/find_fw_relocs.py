import pefile

pe = pefile.PE("windows-drv/motuaw.sys")
target_rva = 0x37DD8
target_va = pe.OPTIONAL_HEADER.ImageBase + target_rva

print(f"Target VA: {hex(target_va)}")

if hasattr(pe, 'DIRECTORY_ENTRY_BASERELOC'):
    for base_reloc in pe.DIRECTORY_ENTRY_BASERELOC:
        for entry in base_reloc.entries:
            if entry.rva == 0:
                continue
            
            # Read the value at the relocation address
            try:
                # It's an x86-64 binary, so pointers are 64-bit
                data = pe.get_memory_mapped_image()[entry.rva:entry.rva+8]
                import struct
                val = struct.unpack('<Q', data)[0]
                if val == target_va:
                    print(f"Found relocation pointing to firmware blob at RVA {hex(entry.rva)}")
                    
                    # Let's find which section this RVA is in
                    for section in pe.sections:
                        if section.VirtualAddress <= entry.rva < section.VirtualAddress + section.Misc_VirtualSize:
                            print(f"  -> In section: {section.Name.decode().strip(chr(0))}")
            except Exception as e:
                pass
else:
    print("No relocations found.")

