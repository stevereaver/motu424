import re

output = []

with open("motu_hw_trace.log", "r") as f:
    start_parsing = False
    for line in f:
        # Start right after the final zero-fill to BAR0 0x0
        # Specifically, the first non-zero write is to 0x0 with 0x7ec582a
        if "region0+0x0, 0x7ec582a" in line:
            start_parsing = True
            output.append("    write_reg(fd, 0, 0x0, 0x7ec582a);")
            continue
            
        if start_parsing:
            # Stop when we hit the last word of the DSP
            if "region0+0xb44, 0x30000890" in line:
                output.append("    write_reg(fd, 0, 0xb44, 0x30000890);")
                break
                
            if "vfio_region_write" in line and "region0" in line:
                m = re.search(r"vfio_region_write.*\+0x([0-9a-fA-F]+), (0x[0-9a-fA-F]+)", line)
                if m:
                    offset = m.group(1)
                    val = m.group(2)
                    output.append(f"    write_reg(fd, 0, 0x{offset}, {val});")

with open("golden_dsp.c", "w") as f:
    f.write("\n".join(output))
