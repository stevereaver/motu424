#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#define BAR1_BASE 0xf7800000

int main() {
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) { perror("open"); return 1; }
    void *map = mmap(0, 0x1000, PROT_READ, MAP_SHARED, fd, BAR1_BASE);
    if (map == MAP_FAILED) { perror("mmap"); return 1; }
    volatile uint32_t *regs = (volatile uint32_t*)map;
    
    printf("Monitoring 0x1C for 50 readings...\n");
    for (int i=0; i<50; i++) {
        uint32_t val = regs[0x1C/4];
        printf("0x1C: 0x%08X\n", val);
        usleep(10000); // 10ms
    }
    
    return 0;
}
