import re

output = []

# Using the raw trace to extract *every single write* to BAR0/BAR1
# that happens *after* the hardware is kick-started at 10914221,
# up until the point the driver starts continuously polling 0x6fec/0x7040.

with open("motu_hw_trace.log", "r") as f:
    start_parsing = False
    for line in f:
        if "10914221" in line:
            start_parsing = True
            output.append("    write_reg(fd, 1, 0x8, 0x10914221); // Kickstart!")
            continue
            
        if start_parsing:
            # Check for the start of the infinite polling loop (wait for DSP ready)
            if "0x6fec" in line or "0x7040" in line:
                break
                
            m_write = re.search(r"vfio_region_write\s+\([^:]+:region(\d+)\+(0x[0-9a-f]+),\s+(0x[0-9a-f]+)", line)
            if m_write:
                bar = m_write.group(1)
                offset = m_write.group(2)
                val = m_write.group(3)
                output.append(f"    write_reg(fd, {bar}, {offset}, {val});")
            
            m_read = re.search(r"vfio_region_read\s+\([^:]+:region(\d+)\+(0x[0-9a-f]+)", line)
            if m_read:
                bar = m_read.group(1)
                offset = m_read.group(2)
                output.append(f"    usleep(100); // Emulate hardware delay for read at {offset}")

with open("golden_sequence.c", "w") as f:
    f.write("\n".join(output))
