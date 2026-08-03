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

uint32_t read_reg(int fd, int bar, uint32_t offset) {
    struct motu_ioctl_data data = { .offset = offset, .value = 0, .write = 0, .bar = bar };
    ioctl(fd, MOTU_IOC_POKE, &data);
    return data.value;
}

int main(int argc, char *argv[]) {
    int fd = open("/dev/motu_poke", O_RDWR);
    if (fd < 0) { perror("open /dev/motu_poke"); return 1; }

    if (argc < 2) {
        printf("Usage: %s <firmware_file>\n", argv[0]);
        return 1;
    }

    // 1. Load FPGA Configuration (The "Body")
    int ffd = open(argv[1], O_RDONLY);
    if (ffd < 0) { perror("open firmware"); return 1; }
    size_t fw_size = lseek(ffd, 0, SEEK_END);
    lseek(ffd, 0, SEEK_SET);
    uint8_t *fw = malloc(fw_size);
    read(ffd, fw, fw_size);
    close(ffd);

    printf("1. Uploading FPGA Firmware to BAR1 (bit-banging)...\n");
    write_reg(fd, 2, 0x4, 0x1); // Initial Reset
    write_reg(fd, 1, 0x300000, 0xE0);
    write_reg(fd, 1, 0x300004, 0xE0);
    write_reg(fd, 1, 0x300008, 0x00);
    
    uint32_t val_prev = 0x40;
    for (size_t i = 0; i < fw_size; i++) {
        uint8_t byte = fw[i];
        for (int bit = 0; bit < 8; bit++) {
            uint32_t data_bit = (byte & (1 << bit)) ? 0x20 : 0x00;
            uint32_t val = 0x40 | data_bit;
            write_reg(fd, 1, 0x300008, val_prev);
            write_reg(fd, 1, 0x300008, val);
            write_reg(fd, 1, 0x300008, val | 0x80);
            val_prev = val;
        }
    }
    write_reg(fd, 1, 0x300004, 0xC0); // Finalize firmware
    write_reg(fd, 2, 0x8, 0x0);       // Finalize BAR2
    printf("\n   Firmware upload complete.\n");

    // 2. Clear Program RAM and Load DSP Program (The "Brain")
    printf("2. Zeroing out DSP RAM...\n");
    for (uint32_t i = 0; i < 0x10000; i++) {
        write_reg(fd, 0, i * 4, 0);
    }
    usleep(10000);

    printf("   Uploading precise DSP Instructions to BAR0...\n");
#include "golden_dsp.c"

    printf("   Applying dynamic DSP patches (includes Routing and Sample Rate)...\n");
#include "golden_dsp_patch.c"

    printf("   DSP upload complete.\n");

    // Fetch the Linux physical address
    uint32_t dma_addr = 0;
    if (ioctl(fd, 0x12345678, &dma_addr) < 0) {
        perror("GET_DMA");
        return 1;
    }
    printf("   Using Linux physical DMA address: 0x%X\n", dma_addr);

    // 3. Kick-start and Configuration
    printf("3. Commanding DSP to boot...\n");
    write_reg(fd, 2, 0x0, 0x0);
    write_reg(fd, 0, 0x3fffc, 0x0);
    write_reg(fd, 2, 0x4, 0x2);
    
    // Give it a moment to stabilize
    usleep(500000); 

    printf("Boot Vector is: 0x%X\n", read_reg(fd, 0, 0x3fffc));
    
    printf("Current Status (Port 1): 0x%X\n", read_reg(fd, 2, 0x0));
    printf("Current Status (Port 2): 0x%X\n", read_reg(fd, 2, 0x4));

    printf("\nDONE! Watch the 24I/O Sync Light.\n");

    free(fw);
    close(fd);
    return 0;
}
