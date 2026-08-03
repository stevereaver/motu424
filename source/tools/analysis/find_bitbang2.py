with open('poke_fpga.c', 'r') as f:
    lines = f.readlines()
    for i, line in enumerate(lines):
        if "uint8_t byte = fw[i];" in line:
            for j in range(i, i+15):
                print(lines[j].strip())
            break
