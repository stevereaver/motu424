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
    printf("Comprehensive Synchronized Fuzzer (0x20 and Port Banks)\n");
    printf("Watch the 24I/O lights. Press ANY KEY if any light goes solid!\n\n");

    // Enable all ports (0x0F) + standard routing (0x277C) + DMA Master Run (0x80)
    uint32_t p_base = 0x000F27FC; 
    write_reg(fd, 1, 0x00, p_base);
    
    // Extracted sample rates and generic clock constants seen in driver code
    uint32_t rates[] = {
        0x0000ac44, // 44.1k
        0x0000bb80, // 48k
        0x00015888, // 88.2k
        0x00017700, // 96k
        0x0002b110, // 176.4k
        0x0002ee00, // 192k
        0x00175f3f, // Seen globally in trace
        0x00000000
    };
    
    // The driver has specific port banks. 
    // We found out earlier that 0x84 is written during initialization.
    // Port banks are often spaced by 0x80 or 0x40. Let's write to 0x84, 0xC4, 0x104, 0x144.
    
    for (int r = 0; r < 8; r++) {
        uint32_t rate = rates[r];
        printf("\n=> Setting rate reg 0x20 to %08X\n", rate);
        write_reg(fd, 1, 0x20, rate);
        
        // Fuzzing the port bank configuration registers.
        // We test individual bits to safely determine the "Sync Enable" bit for the port.
        for (int bit = 0; bit < 32; bit++) {
            uint32_t val = (1 << bit);
            printf("\rTesting Port Banks (0x84, 0xC4, 0x104, 0x144) = %08x ... ", val);
            fflush(stdout);
            
            // Write to all 4 possible port banks
            write_reg(fd, 1, 0x84, val);
            write_reg(fd, 1, 0xC4, val);
            write_reg(fd, 1, 0x104, val);
            write_reg(fd, 1, 0x144, val);
            
            for(int w=0; w<2; w++) {
                if (kbhit()) {
                    printf("\n\nSUCCESS! 0x20=%08x, PortBank=%08x\n", rate, val);
                    goto done;
                }
                usleep(100000); // 200ms
            }
        }
    }
    
done:
    write_reg(fd, 1, 0x84, 0);
    write_reg(fd, 1, 0xC4, 0);
    write_reg(fd, 1, 0x104, 0);
    write_reg(fd, 1, 0x144, 0);
    write_reg(fd, 1, 0x00, 0x000F277C);
    close(fd);
    printf("\nFinished.\n");
    return 0;
}
