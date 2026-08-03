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

void write_reg(int fd, int bar, uint32_t offset, uint32_t value) {
    struct motu_ioctl_data data = { .offset = offset, .value = value, .write = 1, .bar = bar };
    ioctl(fd, MOTU_IOC_POKE, &data);
}

int main() {
    int fd = open("/dev/motu_poke", O_RDWR);
    if (fd < 0) { perror("open"); return 1; }
    
    // Enabling Port 1 in the global config 0x00
    write_reg(fd, 1, 0x00, 0x000F2782); // 0x82 = bit 1 (Port 1) + bit 7 (Enable)
    
    // Now fuzzing the Port 1 specific register bank at 0x80
    for (int i = 16; i < 32; i++) {
        uint32_t val = 0x000F277C | (1 << i);
        printf("Testing Port 1 Bank 0x80 = %08x (Bit %d)\n", val, i);
        write_reg(fd, 1, 0x80, val);
        usleep(500000); 
    }
    
    // Restore
    write_reg(fd, 1, 0x00, 0x000F277C);
    write_reg(fd, 1, 0x80, 0x000F277C);
    close(fd);
    return 0;
}
