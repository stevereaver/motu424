// SPDX-License-Identifier: GPL-2.0

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#define BAR1_BASE 0xf7800000

int main() {
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) return 1;
    void *map = mmap(0, 0x1000, PROT_READ | PROT_WRITE, MAP_SHARED, fd, BAR1_BASE);
    if (map == MAP_FAILED) return 1;
    uint32_t *regs = (uint32_t*)map;
    
    printf("Original:\n");
    printf("0x04: %08X, 0x08: %08X, 0x10: %08X, 0x14: %08X\n", regs[1], regs[2], regs[4], regs[5]);
    
    regs[1] = 0x11111111;
    regs[2] = 0x22222222;
    regs[4] = 0x44444444;
    regs[5] = 0x55555555;
    
    printf("After write:\n");
    printf("0x04: %08X, 0x08: %08X, 0x10: %08X, 0x14: %08X\n", regs[1], regs[2], regs[4], regs[5]);
    
    return 0;
}
