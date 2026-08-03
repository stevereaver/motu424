// SPDX-License-Identifier: GPL-2.0

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <termios.h>

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

int kbhit() {
    struct termios oldt, newt;
    int ch, oldf;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);
    ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);
    if(ch != EOF) { ungetc(ch, stdin); return 1; }
    return 0;
}

int main() {
    int fd = open("/dev/motu_poke", O_RDWR);
    if (fd < 0) { perror("open"); return 1; }
    printf("Exhaustive Clock Top-Bits Test (All Ports Enabled)...\n");
    printf("Watch the 24I/O lights. Press ANY KEY if any light goes solid!\n\n");

    // Enable all ports (0x0F) + standard routing (0x277C) + DMA Master Run (0x80)
    uint32_t base = 0x000F27FC; 
    
    // We will test combinations of bits 16-24 (which is where rate/clock is likely mapped)
    // 9 bits = 512 combinations. 0.2s each = 100 seconds total.
    for (uint32_t top = 0; top < 512; top++) {
        uint32_t val = base | (top << 16);
        printf("\rTesting 0x00 = %08x ... ", val);
        fflush(stdout);
        write_reg(fd, 1, 0x00, val);
        
        for(int w=0; w<2; w++) {
            if (kbhit()) {
                printf("\n\nSUCCESS! magic value is 0x%08x\n", val);
                goto done;
            }
            usleep(100000); // 200ms total
        }
    }
    
done:
    write_reg(fd, 1, 0x00, 0x000F277C);
    close(fd);
    printf("\nFinished.\n");
    return 0;
}
