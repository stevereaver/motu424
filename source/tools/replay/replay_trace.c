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

int fd;

void write_reg(int bar, uint32_t offset, uint32_t value) {
    struct motu_ioctl_data data = { .offset = offset, .value = value, .write = 1, .bar = bar };
    ioctl(fd, MOTU_IOC_POKE, &data);
}

uint32_t read_reg(int bar, uint32_t offset) {
    struct motu_ioctl_data data = { .offset = offset, .value = 0, .write = 0, .bar = bar };
    ioctl(fd, MOTU_IOC_POKE, &data);
    return data.value;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <trace_file>\n", argv[0]);
        return 1;
    }

    fd = open("/dev/motu_poke", O_RDWR);
    if (fd < 0) { perror("open /dev/motu_poke"); return 1; }

    FILE *f = fopen(argv[1], "r");
    if (!f) { perror("fopen"); return 1; }

    char line[256];
    size_t count = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, "vfio_region_write")) {
            int bar;
            uint32_t offset, val;
            if (sscanf(line, "vfio_region_write  (0000:06:01.0:region%d+%x, %x", &bar, &offset, &val) == 3) {
                write_reg(bar, offset, val);
            }
        } else if (strstr(line, "vfio_region_read")) {
            int bar;
            uint32_t offset;
            if (sscanf(line, "vfio_region_read  (0000:06:01.0:region%d+%x", &bar, &offset) == 2) {
                read_reg(bar, offset);
                // Introduce a tiny delay on reads to simulate the hardware polling wait time
                usleep(50);
            }
        }
        
        count++;
        if (count % 100000 == 0) {
            printf("\rReplayed %zu events...", count);
            fflush(stdout);
        }
    }
    printf("\nReplay finished! Events: %zu\n", count);

    fclose(f);
    close(fd);
    return 0;
}
