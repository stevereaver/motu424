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
    
    // Test IO Port 0x00 fuzzing, as outsl sends there. FPP mode usually requires taking a pin low/high first.
    // The previous state of port 0x00 was 0x10.
    
    // Let's just try writing a 1 to bits 0-7 of the I/O port, maybe it's a simple start clock command.
    for (int b=0; b<8; b++) {
        printf("Trying BAR2 0x00 = %08x\n", 1<<b);
        write_reg(fd, 2, 0x00, 1<<b);
        usleep(250000); 
    }
    
    close(fd);
    return 0;
}
