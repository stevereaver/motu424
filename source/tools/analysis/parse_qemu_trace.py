import re

# We're looking for the data bit-banged into 0x300008.
# In FPP mode, usually:
# Bit 7 = Clock
# Bit 6 = Data
# Bit 5 = Config / Status
# (This is a guess, but let's see).

data_bytes = []
current_byte = 0
bit_count = 0

with open("motu_hw_trace.log", "r") as f:
    for line in f:
        if "region1+0x300008" in line:
            m = re.search(r"0x([0-9a-f]+)", line.split(",")[-2])
            if m:
                val = int(m.group(1), 16)
                # Let's assume bit 6 is data and bit 7 is clock.
                # We only care about writes where clock transitions from 0 to 1?
                # Or just look at the unique values.
                pass

# Let's just dump the raw unique writes to 0x300008 in order.
