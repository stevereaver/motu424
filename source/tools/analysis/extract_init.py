import re
output = []
start_parsing = False
with open('motu_hw_trace.log', 'r') as f:
    for line in f:
        if '10914221' in line:
            start_parsing = True
            continue
        
        if start_parsing and 'vfio_region_write' in line:
            m = re.search(r'vfio_region_write\s+\([^:]+:region(\d+)\+(0x[0-9a-f]+),\s+(0x[0-9a-f]+)', line)
            if m:
                bar = m.group(1)
                offset = m.group(2)
                val = m.group(3)
                if offset in ['0x6fec', '0x7040']:
                    break
                output.append(f'    write_reg(fd, {bar}, {offset}, {val});')

with open('init_sequence.txt', 'w') as f:
    f.write('\n'.join(output))
