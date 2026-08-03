import re

output = []

# Extract *every single write* to BAR0 that happens *after* the very last
# write of the DSP upload (offset 0xb44 according to our analysis), 
# up until the point the hardware is kick-started at 10914221.

with open("motu_hw_trace.log", "r") as f:
    start_parsing = False
    for line in f:
        # Start parsing right after the last word of the DSP is written
        if "vfio_region_write" in line and "region0+0xb44" in line and "0x30000890" in line:
            start_parsing = True
            continue
            
        if start_parsing:
            # Stop right when we hit the kickstart
            if "10914221" in line:
                output.append("    write_reg(fd, 1, 0x8, 0x10914221); // Kickstart!")
                break
                
            m_write = re.search(r"vfio_region_write\s+\([^:]+:region(\d+)\+(0x[0-9a-fA-F]+),\s+(0x[0-9a-fA-F]+)", line)
            if m_write:
                bar = m_write.group(1)
                offset = m_write.group(2)
                val = m_write.group(3)
                output.append(f"    write_reg(fd, {bar}, {offset}, {val});")
            
            m_read = re.search(r"vfio_region_read\s+\([^:]+:region(\d+)\+(0x[0-9a-fA-F]+)", line)
            if m_read:
                bar = m_read.group(1)
                offset = m_read.group(2)
                output.append(f"    usleep(100); // Emulate hardware delay for read at {offset}")

with open("golden_sequence.c", "w") as f:
    f.write("\n".join(output))
