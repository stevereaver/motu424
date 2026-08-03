// SPDX-License-Identifier: GPL-2.0

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
    
    printf("Port Statuses (0x28, 0x2C, 0x30, 0x34) across banks:\n");
    for (int b = 0; b < 8; b++) {
        uint32_t base = b * 0x80;
        printf("Bank %d: %08x %08x %08x %08x\n", b, 
            read_reg(fd, 1, base + 0x28),
            read_reg(fd, 1, base + 0x2C),
            read_reg(fd, 1, base + 0x30),
            read_reg(fd, 1, base + 0x34));
    }
    
    close(fd);
    return 0;
}
