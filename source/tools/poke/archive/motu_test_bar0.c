// SPDX-License-Identifier: GPL-2.0

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#define BAR0_BASE 0xf4c00000

int main() {
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) return 1;
    void *map = mmap(0, 0x400000, PROT_READ | PROT_WRITE, MAP_SHARED, fd, BAR0_BASE);
    if (map == MAP_FAILED) {
        perror("mmap");
        return 1;
    }
    uint32_t *dsp_mem = (uint32_t*)map;
    
    printf("Testing BAR0 DSP Memory...\n");
    printf("Original 0x00: %08X\n", dsp_mem[0]);
    printf("Original 0x1000: %08X\n", dsp_mem[0x1000/4]);
    
    dsp_mem[0] = 0xDEADC0DE;
    dsp_mem[0x1000/4] = 0xBEEFCAFE;
    
    printf("After write: 0x00: %08X, 0x1000: %08X\n", dsp_mem[0], dsp_mem[0x1000/4]);
    
    return 0;
}
