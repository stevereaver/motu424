import re

with open("motu_hw_trace.log", "r") as f:
    for line in f:
        if "vfio_region_read" in line and "region0" in line:
            m = re.search(r"region0\+(0x[0-9a-fA-F]+), 4\) = (0x[0-9a-fA-F]+)", line)
            if m:
                offset = m.group(1)
                val = m.group(2)
                # Only show non-zero reads or interesting reads after boot
                if val != "0x0" and int(offset, 16) > 0x6000:
                    print(f"Read {offset} -> {val}")
