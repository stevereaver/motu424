import re

with open("motu_hw_trace.log", "r") as f:
    in_zero_fill = False
    for line in f:
        if "vfio_region_write" in line and "region0+0x0" in line and ", 0x0," in line:
            in_zero_fill = True
            print("Found zero fill start")
        if in_zero_fill:
            if ", 0x0," not in line:
                print("Zero fill ends at:", line.strip())
                in_zero_fill = False
                break
