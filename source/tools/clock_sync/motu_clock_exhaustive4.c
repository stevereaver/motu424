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

int main() {
    int fd = open("/dev/motu_poke", O_RDWR);
    if (fd < 0) { perror("open"); return 1; }

    uint32_t rates[] = {44100, 48000};
    uint32_t polarities[] = {0x00000000, 0x80000000};
    uint32_t enable_masks[] = {0x10FFF, 0x1FFFF};

    printf("Starting Exhaustive Hardware Sync Sweep...\n");

    for (int r = 0; r < 2; r++) {
        for (int p = 0; p < 2; p++) {
            for (int e = 0; e < 2; e++) {
                uint32_t val00 = enable_masks[e] | polarities[p];
                printf("\rRate: %d, FPGA 0x00: %08x ", rates[r], val00);
                fflush(stdout);

                // Set FPGA clock
                write_reg(fd, 1, 0x20, rates[r]);
                // Set FPGA base
                write_reg(fd, 1, 0x00, val00);
                // Set DSP clock
                write_reg(fd, 0, 0x6344, (0x09400000 | rates[r]));

                // Reset and Start all ports
                for(int i=0; i<4; i++) write_reg(fd, 2, i*4, 0x1);
                usleep(50000);
                for(int i=0; i<4; i++) write_reg(fd, 2, i*4, 0x2);

                // Wait for PLL lock attempt
                for(int w=0; w<10; w++) {
                    for(int i=0; i<4; i++) {
                        if (read_reg(fd, 2, i*4) == 0x13) {
                            printf("\nSYNC LOCKED! Port %d is active.\n", i+1);
                            printf("Final Settings: Rate=%d, FPGA 0x00=%08x\n", rates[r], val00);
                            goto done;
                        }
                    }
                    usleep(50000);
                }
            }
        }
    }

    printf("\nSweep complete. No lock found.\n");

done:
    close(fd);
    return 0;
}
