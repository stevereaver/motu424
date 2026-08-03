// SPDX-License-Identifier: GPL-2.0

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>

struct motu_ioctl_data {
    uint32_t offset;
    uint32_t value;
    int write;
    int bar;
};

#define MOTU_IOC_MAGIC 'M'
#define MOTU_IOC_POKE _IOWR(MOTU_IOC_MAGIC, 1, struct motu_ioctl_data)
#define MOTU_IOC_GET_DMA _IOWR(MOTU_IOC_MAGIC, 2, uint32_t[4])

int fd;
uint32_t dma_addrs[4];

void write_reg(int bar, uint32_t offset, uint32_t value) {
    struct motu_ioctl_data data = { .offset = offset, .value = value, .write = 1, .bar = bar };
    ioctl(fd, MOTU_IOC_POKE, &data);
}

uint32_t read_reg(int bar, uint32_t offset) {
    struct motu_ioctl_data data = { .offset = offset, .value = 0, .write = 0, .bar = bar };
    ioctl(fd, MOTU_IOC_POKE, &data);
    return data.value;
}

uint32_t translate(uint32_t val) {
    if ((val & 0xFFFFF000) == 0x10914000) return dma_addrs[0] | (val & 0xFFF);
    if (val == 0xfe870000) return dma_addrs[1];
    if (val == 0x90000000) return dma_addrs[2];
    if ((val & 0xFFFF0000) == 0xbfd70000) return dma_addrs[3] | (val & 0xFFFF);
    if ((val & 0xFFFFF000) == 0xbff92000) return dma_addrs[3] + 0x222000 + (val & 0xFFF);
    return val;
}

int main(int argc, char *argv[]) {
    if (argc < 2) return 1;
    fd = open("/dev/motu_poke", O_RDWR);
    ioctl(fd, MOTU_IOC_GET_DMA, dma_addrs);

    FILE *f = fopen(argv[1], "r");
    char line[256];
    int in_sync_phase = 0;

    while (fgets(line, sizeof(line), f)) {
        // Only start replaying from the point where the card is initialized
        if (strstr(line, "0xb44, 0x30000890")) in_sync_phase = 1;
        if (!in_sync_phase) continue;

        if (strstr(line, "vfio_region_write")) {
            int bar; uint32_t off, val;
            if (sscanf(line, "vfio_region_write  (0000:06:01.0:region%d+%x, %x", &bar, &off, &val) == 3) {
                write_reg(bar, off, translate(val));
            }
        } else if (strstr(line, "vfio_region_read")) {
            int bar; uint32_t off, target_val;
            if (sscanf(line, "vfio_region_read  (0000:06:01.0:region%d+%x, 4) = %x", &bar, &off, &target_val) == 3) {
                // Smart Poll: If the trace was waiting for a specific status, we wait too!
                if (off == 0x6fec || off == 0x7040 || off == 0x3fffc) {
                    int retry = 0;
                    while (read_reg(bar, off) != target_val && retry++ < 1000) {
                        usleep(10);
                    }
                } else {
                    read_reg(bar, off);
                }
            }
        }
        
        // If we hit the final "Locked" status in the trace, we are done!
        if (strstr(line, "region2+0x0, 4) = 0x13")) {
            printf("Target Status 0x13 reached in trace replay!\n");
            break;
        }
    }

    printf("Final Hardware Status: P1=%x, P2=%x\n", read_reg(2, 0x0), read_reg(2, 0x4));
    fclose(f);
    close(fd);
    return 0;
}
