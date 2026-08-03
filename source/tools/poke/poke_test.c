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
};

#define MOTU_IOC_MAGIC 'M'
#define MOTU_IOC_POKE _IOWR(MOTU_IOC_MAGIC, 1, struct motu_ioctl_data)

uint32_t read_reg(int fd, uint32_t offset) {
    struct motu_ioctl_data data = { .offset = offset, .write = 0 };
    ioctl(fd, MOTU_IOC_POKE, &data);
    return data.value;
}

void write_reg(int fd, uint32_t offset, uint32_t value) {
    struct motu_ioctl_data data = { .offset = offset, .value = value, .write = 1 };
    ioctl(fd, MOTU_IOC_POKE, &data);
}

int main() {
    int fd = open("/dev/motu_poke", O_RDWR);
    if (fd < 0) { perror("open"); return 1; }
    
    printf("Original 0x00: %08x\n", read_reg(fd, 0x00));
    
    // We want to loop over bits to see if we can catch the sync light
    for (int port=0; port<4; port++) {
        for (int b=16; b<32; b++) {
            uint32_t val = 0x000F277C | (1<<port) | (1<<b) | 0x80;
            printf("Trying 0x00 = %08x (Port %d, Bit %d)\n", val, port, b);
            write_reg(fd, 0x00, val);
            usleep(500000); // 0.5 sec to look at light
        }
    }
    
    close(fd);
    return 0;
}
