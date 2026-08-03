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
    int ch;
    int oldf;

    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

    ch = getchar();

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);

    if(ch != EOF) {
        ungetc(ch, stdin);
        return 1;
    }
    return 0;
}

int main() {
    int fd = open("/dev/motu_poke", O_RDWR);
    if (fd < 0) { perror("open"); return 1; }
    
    printf("Testing Port 1 Clock Base Configurations...\n");
    
    // The previous test only toggled a single bit. Sometimes a 2-bit or 3-bit pattern is needed.
    // E.g., setting the port format to 24I/O specific protocol.
    
    // We are going to test standard clock configurations for MOTU PCIe-424.
    // 0x00277C is some base for port routing.
    // We'll iterate through all 256 possible combinations of bits 16-23 (which likely control clock generation/routing)
    
    uint32_t base = 0x0000277C | (1<<1) | 0x80; // Port 1 + Master Enable
    
    for (int i = 0; i < 256; i++) {
        uint32_t val = base | (i << 16);
        printf("Testing 0x00 = %08x (Bits 16-23: 0x%02X)...\n", val, i);
        write_reg(fd, 1, 0x00, val);
        
        for(int w=0; w<10; w++) {
            if (kbhit()) {
                printf("\nSUCCESS! You pressed a key. The magic value is 0x%08x\n", val);
                getchar(); // consume
                goto done;
            }
            usleep(100000); // 1 sec total per value
        }
    }

done:
    write_reg(fd, 1, 0x00, 0x000F277C);
    close(fd);
    return 0;
}
