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

void write_reg(int fd, int bar, uint32_t offset, uint32_t value) {
    struct motu_ioctl_data data = { .offset = offset, .value = value, .write = 1, .bar = bar };
    ioctl(fd, MOTU_IOC_POKE, &data);
}

int main(int argc, char *argv[]) {
    int fd = open("/dev/motu_poke", O_RDWR);
    if (fd < 0) { perror("open"); return 1; }

    int ffd = open("pci_424_fw.bin", O_RDONLY);
    if (ffd < 0) { perror("open firmware"); return 1; }
    size_t fw_size = lseek(ffd, 0, SEEK_END);
    lseek(ffd, 0, SEEK_SET);
    uint8_t *fw = malloc(fw_size);
    read(ffd, fw, fw_size);
    close(ffd);

    printf("Uploading FPGA Firmware with SLOW timing...\n");
    write_reg(fd, 2, 0x4, 0x1); 
    write_reg(fd, 1, 0x300000, 0xE0);
    write_reg(fd, 1, 0x300004, 0xE0);
    write_reg(fd, 1, 0x300008, 0x00);
    
    for (size_t i = 0; i < fw_size; i++) {
        uint8_t byte = fw[i];
        for (int bit = 0; bit < 8; bit++) {
            uint32_t data_bit = (byte & (1 << bit)) ? 0x20 : 0x00;
            uint32_t val = 0x40 | data_bit;
            write_reg(fd, 1, 0x300008, val);
            write_reg(fd, 1, 0x300008, val);
            write_reg(fd, 1, 0x300008, val | 0x80);
            // Minimal delay to ensure hardware sees the transition
            for(int d=0; d<100; d++) __asm__("nop");
        }
        if (i % 1000 == 0) printf("\rProgress: %zu/%zu", i, fw_size);
    }
    write_reg(fd, 1, 0x300004, 0xC0); 
    write_reg(fd, 2, 0x8, 0x0);
    printf("\nFirmware complete.\n");

    free(fw);
    close(fd);
    return 0;
}
