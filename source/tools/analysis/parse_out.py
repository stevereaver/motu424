with open("motu_hw_trace.log", "r") as f:
    events = []
    capture = False
    for line in f:
        if '0x10914221' in line:
            capture = True
        if capture:
            events.append(line.strip())
            if '0x13' in line and 'region2+0x0' in line:
                break
    for i in range(len(events)):
        print(events[i])
