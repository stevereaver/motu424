import re

with open("motuaw.asm", "r") as f:
    for line in f:
        # Looking for constants that might be the 0x20 clock register configurations.
        if "mov" in line and ("0xac44" in line or "0xbb80" in line or "0x17700" in line):
            print(line.strip())
