import re

output = []
start_parsing = False
with open("motu_hw_trace.log", "r") as f:
    for line in f:
        if "10914221" in line:
            start_parsing = True
            continue
        
        if start_parsing:
            if "0x6fec" in line:
                break
                
            if "vfio_region_write" in line:
                m = re.search(r"vfio_region_write\s+\([^:]+:region(\d+)\+(0x[0-9a-f]+),\s+(0x[0-9a-f]+)", line)
                if m:
                    bar = m.group(1)
                    offset = m.group(2)
                    val = m.group(3)
                    output.append(f"    write_reg(fd, {bar}, {offset}, {val});")
            elif "vfio_region_read" in line:
                m = re.search(r"vfio_region_read\s+\([^:]+:region(\d+)\+(0x[0-9a-f]+)", line)
                if m:
                    bar = m.group(1)
                    offset = m.group(2)
                    output.append(f"    // Read wait loop")
                    output.append(f"    // read_reg(fd, {bar}, {offset});")
                    output.append(f"    usleep(1000);")

with open("init_seq_v2.txt", "w") as f:
    f.write("\n".join(output))
