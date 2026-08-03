import re
count = 0
found = False
with open("motu_hw_trace.log", "r") as f:
    for line in f:
        if "0xb44, 0x30000890" in line:
            found = True
        
        if found:
            if "vfio_region_read" in line:
                m = re.search(r"region0\+(0x[0-9a-fA-F]+), 4\) = (0x[0-9a-fA-F]+)", line)
                if m:
                    print(f"Wait Read: {m.group(1)} -> {m.group(2)}")
            if "10914221" in line:
                break
