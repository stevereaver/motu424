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
#define MOTU_IOC_GET_DMA _IOR(MOTU_IOC_MAGIC, 2, uint32_t)

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

    // 1. Load FPGA Configuration
    int ffd = open("pci_424_fw.bin", O_RDONLY);
    if (ffd < 0) { perror("open firmware"); return 1; }
    size_t fw_size = lseek(ffd, 0, SEEK_END);
    lseek(ffd, 0, SEEK_SET);
    uint8_t *fw = malloc(fw_size);
    read(ffd, fw, fw_size);
    close(ffd);

    printf("1. Uploading FPGA Firmware...\n");
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
        }
    }
    write_reg(fd, 1, 0x300004, 0xC0); 
    write_reg(fd, 2, 0x8, 0x0);
    printf("   Firmware upload complete.\n");

    // 2. Load DSP Brain
    printf("2. Clearing and Uploading DSP Brain...\n");
    for (uint32_t i = 0; i < 0x10000; i++) write_reg(fd, 0, i * 4, 0);
    
#include "golden_dsp.c"
#include "golden_dsp_patch.c"

    // 3. Kickstart DSP with DMA translation
    uint32_t dma_addr = 0;
    ioctl(fd, MOTU_IOC_GET_DMA, &dma_addr);
    printf("3. Kickstarting DSP with Linux DMA at 0x%X\n", dma_addr);

    // Initial Execution Command
    write_reg(fd, 2, 0x0, 0x0);
    write_reg(fd, 0, 0x3fffc, 0x0);
    write_reg(fd, 2, 0x4, 0x2);
    
    usleep(500000); 
    printf("   Boot Vector Check: 0x%X\n", read_reg(fd, 0, 0x3fffc));

    // 4. Full Safe Replay of Configuration Sequence
    printf("4. Sending exhaustive configuration (%d steps)...\n", 24678);

#include "golden_sequence.c"

    printf("\nDONE! Card should now be live. Watch the lights.\n");
    printf("Starting background IRQ ACK loop to keep DSP active...\n");
    
    while(1) {
        write_reg(fd, 1, 0x400000, 0x10);
        usleep(500);
    }

    free(fw);
    close(fd);
    return 0;
}
