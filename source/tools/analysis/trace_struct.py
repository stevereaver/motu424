import sys

with open("motuaw.asm", "r") as f:
    lines = f.readlines()
    
    print("Uses of 0x5e:")
    for i, line in enumerate(lines):
        if "5e(%r" in line:
            print(f"{lines[i].strip()}")
            
    print("\nUses of 0x63:")
    for i, line in enumerate(lines):
        if "63(%r" in line:
            print(f"{lines[i].strip()}")
