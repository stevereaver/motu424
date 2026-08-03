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

void write_reg(int fd, uint32_t offset, uint32_t value) {
    struct motu_ioctl_data data = { .offset = offset, .value = value, .write = 1 };
    ioctl(fd, MOTU_IOC_POKE, &data);
}

int main() {
    int fd = open("/dev/motu_poke", O_RDWR);
    if (fd < 0) { perror("open"); return 1; }
    
    // Some devices require both 0x00 and 0x04 to be initialized.
    // Try setting base port 1 on 0x00 and fuzzing 0x04.
    uint32_t base = 0x000F277C | (1<<1) | 0x80;
    write_reg(fd, 0x00, base);
    usleep(100000);
    
    for (int b=0; b<32; b++) {
        uint32_t val = (1 << b);
        printf("Trying 0x04 = %08x (Bit %d)\n", val, b);
        write_reg(fd, 0x04, val);
        usleep(250000); 
    }
    write_reg(fd, 0x04, 0);

    for (int b=0; b<32; b++) {
        uint32_t val = (1 << b);
        printf("Trying 0x08 = %08x (Bit %d)\n", val, b);
        write_reg(fd, 0x08, val);
        usleep(250000); 
    }
    write_reg(fd, 0x08, 0);
    
    write_reg(fd, 0x00, 0);
    close(fd);
    return 0;
}
