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

int main(int argc, char *argv[]) {
    int fd = open("/dev/motu_poke", O_RDWR);
    if (fd < 0) { perror("open"); return 1; }

    uint32_t rate_44 = 0x0940ac44;
    uint32_t rate_48 = 0x0940bb80;
    
    if (argc > 1 && argv[1][0] == '4') {
        printf("Setting DSP to 44.1kHz...\n");
        write_reg(fd, 0, 0x6344, rate_44);
    } else {
        printf("Setting DSP to 48kHz...\n");
        write_reg(fd, 0, 0x6344, rate_48);
    }

    // Toggle Port Enable bits in routing register
    printf("Enabling Ports 1-4 in DSP Routing...\n");
    write_reg(fd, 0, 0x277c, 0x025861e1); // Trace value
    
    // Send IRQ Ack to start the engine
    write_reg(fd, 1, 0x400000, 0x10);

    close(fd);
    return 0;
}
