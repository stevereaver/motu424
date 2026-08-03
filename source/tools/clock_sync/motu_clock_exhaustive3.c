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
    printf("Full Register 0x20 vs 0x00 Sync Fuzzer\n");
    printf("Watch the 24I/O lights. Press ANY KEY if any light goes solid!\n\n");

    // Standard base: all ports (0x0F) + master run (0x80)
    uint32_t p_base = 0x000F27FC; 
    
    // Extracted sample rates and generic clock constants seen in driver code
    uint32_t rates[] = {
        0x0000ac44, // 44.1k
        0x0000bb80, // 48k
        0x00015888, // 88.2k
        0x00017700, // 96k
        0x0002b110, // 176.4k
        0x0002ee00, // 192k
        0x00175f3f, // Seen globally in trace
        0x00175fbf, // Modified global trace
        0x00000000
    };
    
    // Exhaustive top-bits in 0x00
    for (int r = 0; r < 9; r++) {
        uint32_t rate = rates[r];
        printf("\n=> Setting rate reg 0x20 to %08X\n", rate);
        write_reg(fd, 1, 0x20, rate);
        
        for (uint32_t top = 0; top < 256; top++) {
            uint32_t val = p_base | (top << 16);
            printf("\rTesting 0x00 = %08x ... ", val);
            fflush(stdout);
            write_reg(fd, 1, 0x00, val);
            
            for(int w=0; w<2; w++) {
                if (kbhit()) {
                    printf("\n\nSUCCESS! 0x20=%08x, 0x00=%08x\n", rate, val);
                    goto done;
                }
                usleep(100000); // 200ms
            }
        }
    }
    
done:
    write_reg(fd, 1, 0x00, 0x000F277C);
    close(fd);
    printf("\nFinished.\n");
    return 0;
}
