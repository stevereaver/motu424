import os, fcntl, struct

try:
    fd = os.open('/dev/motu_poke', os.O_RDWR)
    def read_reg(bar, off):
        data = bytearray(struct.pack('IIii', off, 0, 0, bar))
        fcntl.ioctl(fd, 0xc0104d01, data)
        return struct.unpack('IIii', data)[1]

    print("BAR2 (Ports):")
    for i in range(4):
        off = i * 4
        print(f"  Port {i+1} (0x{off:x}): {hex(read_reg(2, off))}")

    print("BAR1 (FPGA):")
    print(f"  0x00 (Master): {hex(read_reg(1, 0x0))}")
    print(f"  0x08 (Kickstart): {hex(read_reg(1, 0x8))}")
    print(f"  0x20 (Rate): {hex(read_reg(1, 0x20))}")
    print(f"  0x1c (DMA Counter): {read_reg(1, 0x1c)}")

    print("BAR0 (DSP):")
    print(f"  0x3fffc (Boot Vector): {hex(read_reg(0, 0x3fffc))}")
    print(f"  0x6344 (DSP Rate): {hex(read_reg(0, 0x6344))}")
    print(f"  0x277c (Routing 1): {hex(read_reg(0, 0x277c))}")
    print(f"  0x2784 (Routing 2): {hex(read_reg(0, 0x2784))}")
    print(f"  0x8348 (Slot Map): {hex(read_reg(0, 0x8348))}")
except Exception as e:
    print(f"Error: {e}")
