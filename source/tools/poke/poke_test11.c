#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

struct motu_ioctl_data {
    uint32_t offset;
    uint32_t value;
    int write;
    int bar;
};

#define MOTU_IOC_MAGIC 'M'
#define MOTU_IOC_POKE _IOWR(MOTU_IOC_MAGIC, 1, struct motu_ioctl_data)

uint32_t read_reg(int fd, int bar, uint32_t offset) {
    struct motu_ioctl_data data = { .offset = offset, .write = 0, .bar = bar };
    ioctl(fd, MOTU_IOC_POKE, &data);
    return data.value;
}

int main() {
    int fd = open("/dev/motu_poke", O_RDWR);
    if (fd < 0) { perror("open"); return 1; }
    
    printf("Scanning BAR1 for mixer RAM blocks...\n");
    for (uint32_t i = 0; i < 0x100000; i += 0x1000) {
        uint32_t val = read_reg(fd, 1, i);
        if (val != 0 && val != 0xffffffff) {
            printf("Non-zero data at BAR1 0x%08X: %08x\n", i, val);
        }
    }
    
    close(fd);
    return 0;
}
