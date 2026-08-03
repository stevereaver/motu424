import time
import struct
import os

BAR1_BASE = 0xf7800000
fd = os.open("/dev/mem", os.O_RDWR | os.O_SYNC)
mem = os.pread(fd, 4, BAR1_BASE + 0x1C)
count1 = struct.unpack("<I", mem)[0]
print(f"Initial Count: 0x{count1:08x}")
time.sleep(1)
mem = os.pread(fd, 4, BAR1_BASE + 0x1C)
count2 = struct.unpack("<I", mem)[0]
print(f"Count after 1s: 0x{count2:08x}")
print(f"Diff: {count2 - count1}")
