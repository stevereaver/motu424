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
    
    printf("Testing AMCC registers...\n");
    printf("Original 0x34: %08X, 0x3C: %08X\n", regs[0x34/4], regs[0x3C/4]);
    
    regs[0x34/4] = 0xDEADBEEF;
    regs[0x3C/4] = 0xCAFEBABE;
    
    printf("After write: 0x34: %08X, 0x3C: %08X\n", regs[0x34/4], regs[0x3C/4]);
    
    return 0;
}
