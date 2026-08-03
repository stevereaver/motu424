import re

with open("motuaw.asm", "r") as f:
    for line in f:
        if "47dd8" in line and ".text" not in line and "bad" not in line:
            print(line.strip())
