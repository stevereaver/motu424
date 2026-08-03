import re

with open("motuaw.asm", "r") as f:
    for line in f:
        # Looking for any switch jump table that checks a value around 0x803
        if "cmp" in line and "$0x8" in line:
            print(line.strip())
