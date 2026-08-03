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
    
    printf("Read Heads (0x1C) across banks:\n");
    for (int b = 0; b < 8; b++) {
        uint32_t off = b * 0x80 + 0x1C;
        printf("Bank %d (0x%03X): %08x\n", b, off, read_reg(fd, 1, off));
    }
    
    close(fd);
    return 0;
}
