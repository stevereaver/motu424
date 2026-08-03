import re

def extract_writes(filename):
    writes = []
    with open(filename, 'r') as f:
        for line in f:
            if 'vfio_region_write' in line:
                m = re.search(r'\([^:]+:(region\d+\+0x[0-9a-fA-F]+), (0x[0-9a-fA-F]+)', line)
                if m:
                    reg = m.group(1)
                    val = m.group(2)
                    if '0x300008' in reg or '0x400000' in reg:
                        continue # Skip bitbanging and IRQ acks
                    writes.append((reg, val))
    return writes

w1 = extract_writes('motu_hw_trace.log')
w2 = extract_writes('motu_hw_trace2.log')

# Just compare the lengths and first differences
print(f"Trace 1 non-polling writes: {len(w1)}")
print(f"Trace 2 non-polling writes: {len(w2)}")

# Align and diff
import difflib
diff = list(difflib.context_diff(
    [f"{r}={v}" for r,v in w1],
    [f"{r}={v}" for r,v in w2],
    n=0
))

if len(diff) == 0:
    print("The sequences are EXACTLY IDENTICAL.")
else:
    print(f"Found {len(diff)} differences. First few:")
    for line in diff[:30]:
        print(line)
