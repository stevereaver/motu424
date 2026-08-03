import re
output = []
zero_fill_started = False
dsp_started = False

with open("extract_dsp.txt", "r") as f:
    for line in f:
        m = re.search(r"vfio_region_write\s+\([^:]+:region0\+(0x[0-9a-fA-F]+),\s+(0x[0-9a-fA-F]+)", line)
        if m:
            offset = int(m.group(1), 16)
            val = int(m.group(2), 16)
            
            if offset == 0 and val == 0:
                zero_fill_started = True
            
            if offset == 0 and val == 0x7ec582a:
                dsp_started = True
            
            if dsp_started:
                if offset == 0xb44:
                    print("DSP End found")
                # stop if offset is way out of bounds
                if offset > 0x10000:
                    break
                output.append(val)

with open("pci_424_dsp2.bin", "wb") as f:
    for val in output:
        f.write(val.to_bytes(4, byteorder='little'))
