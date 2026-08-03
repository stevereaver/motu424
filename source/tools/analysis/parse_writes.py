import re

with open("motuaw.asm", "r") as f:
    for line in f:
        # Looking for values written to 0x20 offset (which is the Clock Register)
        if "20(%r" in line and "mov" in line and "$" in line:
            print(line.strip())
