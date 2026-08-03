import re

with open("motuaw.asm", "r") as f:
    lines = f.readlines()

for i, line in enumerate(lines):
    # Look for shr / sar inside a small loop
    if "shr" in line or "sar" in line or "shl" in line:
        # Check if surrounding lines have test / and / or / out / in / mov to mem
        context = "".join(lines[max(0, i-5):min(len(lines), i+6)])
        if "test" in context and ("or" in context or "and" in context) and ("out" in context or "mov" in context):
            # Might be a bitbang loop
            # check for typical FPGA sizes
            if "2b2f8" in context.lower() or "40000" in context.lower():
                print(f"Match around line {i}:")
                print(context)
                print("---")
