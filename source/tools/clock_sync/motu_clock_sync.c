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
    printf("Starting Clock + Port Sync Finder...\n");
    printf("Watch the 24I/O Clock (44.1/48) lights. Press ANY KEY if it goes solid!\n\n");

    uint32_t rates[] = {44100, 48000, 88200, 96000};
    
    for (int r = 0; r < 4; r++) {
        uint32_t rate = rates[r];
        printf("\n===================================\n");
        printf("Setting Rate Register (0x20) to %d (0x%08X)\n", rate, rate);
        write_reg(fd, 1, 0x20, rate);
        
        for (int port = 0; port < 4; port++) {
            uint32_t port_base = 0x0000277C | (1<<port) | 0x80;
            for (int i = 16; i < 32; i++) {
                uint32_t val = port_base | (1 << i);
                printf("\rTesting Port %d (Bit %d) : 0x00 = %08x ...  ", port+1, i, val);
                fflush(stdout);
                write_reg(fd, 1, 0x00, val);
                
                // Wait briefly for PLL to lock (400ms per try)
                for(int w=0; w<4; w++) {
                    if (kbhit()) {
                        printf("\n\nSUCCESS! Rate=%d, PortConfig=0x%08x\n", rate, val);
                        goto done;
                    }
                    usleep(100000); 
                }
            }
        }
    }

done:
    write_reg(fd, 1, 0x00, 0x000F277C);
    close(fd);
    return 0;
}
