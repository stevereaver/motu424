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
    
    printf("BAR1 MMIO Dump via motu_poke:\n");
    for (uint32_t i = 0; i < 0x40; i += 4) {
        printf("0x%02X: %08x\n", i, read_reg(fd, 1, i));
    }
    
    printf("\nBAR2 IO Ports Dump via motu_poke:\n");
    for (uint32_t i = 0; i < 0x10; i += 4) {
        printf("0x%02X: %08x\n", i, read_reg(fd, 2, i));
    }
    
    close(fd);
    return 0;
}
