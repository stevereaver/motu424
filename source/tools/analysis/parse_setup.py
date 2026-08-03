with open('motu_hw_trace.log', 'r') as f:
    for line in f:
        if '0x300000, 0xe0' in line:
            print(line.strip())
            break
