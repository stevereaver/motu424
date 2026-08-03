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
    
    printf("Starting MOTU Sync Finder...\n");
    printf("Watch the 24I/O Sync light. Press ANY KEY immediately if it goes solid!\n\n");
    
    // The base 0x000F277C has all 4 ports enabled (0x0F) and some other bits (0x277C).
    // Let's iterate over every single bit from 16 to 31 to see if it sets the clock mode.
    
    uint32_t base = 0x000F277C;
    
    for (int i = 16; i < 32; i++) {
        uint32_t val = base ^ (1 << i);
        printf("Testing 0x00 = %08x (Toggling bit %d)...\n", val, i);
        write_reg(fd, 1, 0x00, val);
        
        // Wait 1.5 seconds per bit to give the sync light time to react
        for(int w=0; w<15; w++) {
            if (kbhit()) {
                printf("\nSUCCESS! You pressed a key. The magic value is 0x%08x\n", val);
                getchar(); // consume
                goto done;
            }
            usleep(100000);
        }
    }

    // Try port specific combinations
    printf("\nTrying isolated Port 1 combinations...\n");
    for (int i = 16; i < 32; i++) {
        uint32_t val = 0x00000002 | (1 << i) | 0x80; // Port 1 + Enable
        printf("Testing 0x00 = %08x (Isolated Port 1 + bit %d)...\n", val, i);
        write_reg(fd, 1, 0x00, val);
        
        for(int w=0; w<15; w++) {
            if (kbhit()) {
                printf("\nSUCCESS! You pressed a key. The magic value is 0x%08x\n", val);
                getchar(); // consume
                goto done;
            }
            usleep(100000);
        }
    }

    printf("\nFinished fuzzing without finding sync. Restoring 0x000F277C.\n");

done:
    write_reg(fd, 1, 0x00, 0x000F277C);
    close(fd);
    return 0;
}
