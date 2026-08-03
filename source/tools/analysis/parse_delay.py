# Re-evaluate the entire event sequence

events = []
with open('motu_hw_trace.log', 'r') as f:
    for line in f:
        if 'vfio_region_write' in line or 'vfio_region_read' in line:
            events.append(line.strip())

for i, ev in enumerate(events):
    if '0x300004, 0xc0' in ev:
        print("--- FPGA Finish ---")
        for j in range(i+1, i+20):
            print(events[j])
        break
