import re

with open("poke_fpga.c", "r") as f:
    code = f.read()

new_code = re.sub(
    r"write_reg\(fd, 1, 0x300008, val\);\s+write_reg\(fd, 1, 0x300008, val \| 0x80\);\s+write_reg\(fd, 1, 0x300008, val\);",
    r"write_reg(fd, 1, 0x300008, val_prev);\n            write_reg(fd, 1, 0x300008, val);\n            write_reg(fd, 1, 0x300008, val | 0x80);\n            val_prev = val;",
    code
)

new_code = new_code.replace("uint8_t byte = fw[i];", "uint8_t byte = fw[i];\n        static uint32_t val_prev = 0x40; // Init to base low")

with open("poke_fpga.c", "w") as f:
    f.write(new_code)
