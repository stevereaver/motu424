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
    
    uint32_t base_20 = 0x175f3f; // Seen in dump
    printf("Fuzzing 0x20 (Clock/Rate) around base %08x\n", base_20);
    
    for (int i = 0; i < 32; i++) {
        uint32_t val = base_20 ^ (1 << i);
        printf("Testing 0x20 = %08x (Toggle bit %d)\n", val, i);
        write_reg(fd, 1, 0x20, val);
        usleep(300000);
    }
    
    // Also try writing standard sample rates / dividers
    uint32_t rates[] = {44100, 48000, 88200, 96000, 176400, 192000, 2083, 2267};
    for (int i=0; i<8; i++) {
        printf("Testing 0x20 = %08x (Rate %d)\n", rates[i], rates[i]);
        write_reg(fd, 1, 0x20, rates[i]);
        usleep(300000);
    }
    
    write_reg(fd, 1, 0x20, base_20);
    write_reg(fd, 1, 0x00, 0x000F277C);
    close(fd);
    return 0;
}
