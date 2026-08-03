import re

with open("motuaw.asm", "r") as f:
    lines = f.readlines()
    idx = 0
    while idx < len(lines):
        if "outsl" in lines[idx]:
            # Print a window around the outsl
            print(f"--- OUTSL at {lines[idx].split(':')[0].strip()} ---")
            start = max(0, idx - 10)
            end = min(len(lines), idx + 10)
            for i in range(start, end):
                print(lines[i].strip())
            print()
        idx += 1
