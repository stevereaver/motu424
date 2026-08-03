// SPDX-License-Identifier: GPL-2.0
/*
 * poke.c - Unified MOTU PCI-424 register poke/exploration tool.
 *
 * Requires the motu_poke_driver kernel module (source/tools/drivers/).
 *
 * Usage:
 *   poke read  <bar> <offset>              Read a 32-bit register
 *   poke write <bar> <offset> <value>      Write a 32-bit register
 *   poke scan <bar> [start] [end]           Scan a BAR for non-zero values
 *   poke monitor <bar> <offset> [count]    Monitor a register (read repeatedly)
 *   poke fpga <firmware.rbf>                Load FPGA bitstream + DSP program
 *
 * Examples:
 *   poke read  1 0x00                       Read BAR1 register 0x00
 *   poke write 1 0x00 0x000F2782            Write to BAR1 register 0x00
 *   poke scan  1 0x0 0x100000               Scan BAR1 for non-zero values
 *   poke monitor 1 0x1C 50                   Monitor BAR1 0x1C, 50 reads
 *   poke fpga ../shared/altera424b.rbf      Load FPGA firmware
 *
 * Bar mapping:
 *   0 = BAR0 (4MB, DSP program/data memory)
 *   1 = BAR1 (8MB, control registers)
 *   2 = BAR2 (16 bytes, FPGA config port)
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
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
#define MOTU_IOC_POKE  _IOWR(MOTU_IOC_MAGIC, 1, struct motu_ioctl_data)
#define MOTU_IOC_GET_DMA _IOR(MOTU_IOC_MAGIC, 2, uint32_t)

static int g_fd = -1;

static uint32_t read_reg(int bar, uint32_t offset) {
    struct motu_ioctl_data data = {
        .offset = offset, .value = 0, .write = 0, .bar = bar
    };
    if (ioctl(g_fd, MOTU_IOC_POKE, &data) < 0) {
        perror("ioctl POKE read");
        return 0xFFFFFFFF;
    }
    return data.value;
}

static void write_reg(int bar, uint32_t offset, uint32_t value) {
    struct motu_ioctl_data data = {
        .offset = offset, .value = value, .write = 1, .bar = bar
    };
    if (ioctl(g_fd, MOTU_IOC_POKE, &data) < 0) {
        perror("ioctl POKE write");
    }
}

static int parse_bar(const char *str) {
    int bar = atoi(str);
    if (bar < 0 || bar > 2) {
        fprintf(stderr, "Invalid BAR %d (must be 0, 1, or 2)\n", bar);
        exit(1);
    }
    return bar;
}

static uint32_t parse_hex(const char *str) {
    return (uint32_t)strtoul(str, NULL, 0);
}

static void cmd_read(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Usage: poke read <bar> <offset>\n");
        exit(1);
    }
    int bar = parse_bar(argv[2]);
    uint32_t offset = parse_hex(argv[3]);
    uint32_t val = read_reg(bar, offset);
    printf("BAR%d 0x%08X = 0x%08X\n", bar, offset, val);
}

static void cmd_write(int argc, char *argv[]) {
    if (argc < 5) {
        fprintf(stderr, "Usage: poke write <bar> <offset> <value>\n");
        exit(1);
    }
    int bar = parse_bar(argv[2]);
    uint32_t offset = parse_hex(argv[3]);
    uint32_t value = parse_hex(argv[4]);
    write_reg(bar, offset, value);
    printf("BAR%d 0x%08X = 0x%08X (written)\n", bar, offset, value);
}

static void cmd_scan(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: poke scan <bar> [start] [end]\n");
        exit(1);
    }
    int bar = parse_bar(argv[2]);
    uint32_t start = (argc > 3) ? parse_hex(argv[3]) : 0;
    uint32_t end   = (argc > 4) ? parse_hex(argv[4]) : 0x100000;
    uint32_t step  = (bar == 0) ? 4 : 0x1000;

    printf("Scanning BAR%d 0x%08X - 0x%08X (step 0x%X)...\n",
           bar, start, end, step);
    int found = 0;
    for (uint32_t off = start; off < end; off += step) {
        uint32_t val = read_reg(bar, off);
        if (val != 0 && val != 0xFFFFFFFF) {
            printf("  BAR%d 0x%08X = 0x%08X\n", bar, off, val);
            found++;
        }
    }
    printf("Scan complete: %d non-zero registers found.\n", found);
}

static void cmd_monitor(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Usage: poke monitor <bar> <offset> [count]\n");
        exit(1);
    }
    int bar = parse_bar(argv[2]);
    uint32_t offset = parse_hex(argv[3]);
    int count = (argc > 4) ? atoi(argv[4]) : 50;
    int interval_us = 10000; /* 10ms */

    printf("Monitoring BAR%d 0x%08X (%d reads, %dms interval)...\n",
           bar, offset, count, interval_us / 1000);
    for (int i = 0; i < count; i++) {
        uint32_t val = read_reg(bar, offset);
        printf("[%3d] 0x%08X\n", i, val);
        usleep(interval_us);
    }
}

static void cmd_fpga(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: poke fpga <firmware.rbf>\n");
        exit(1);
    }

    /* Load FPGA bitstream */
    int ffd = open(argv[2], O_RDONLY);
    if (ffd < 0) { perror("open firmware"); exit(1); }
    size_t fw_size = lseek(ffd, 0, SEEK_END);
    lseek(ffd, 0, SEEK_SET);
    uint8_t *fw = malloc(fw_size);
    if (!fw) { perror("malloc"); exit(1); }
    read(ffd, fw, fw_size);
    close(ffd);

    printf("1. Uploading FPGA bitstream (%zu bytes)...\n", fw_size);
    write_reg(2, 0x4, 0x1); /* Reset */
    write_reg(1, 0x300000, 0xE0);
    write_reg(1, 0x300004, 0xE0);
    write_reg(1, 0x300008, 0x00);

    uint32_t val_prev = 0x40;
    for (size_t i = 0; i < fw_size; i++) {
        uint8_t byte = fw[i];
        for (int bit = 0; bit < 8; bit++) {
            uint32_t data_bit = (byte & (1 << bit)) ? 0x20 : 0x00;
            uint32_t val = 0x40 | data_bit;
            write_reg(1, 0x300008, val_prev);
            write_reg(1, 0x300008, val);
            write_reg(1, 0x300008, val | 0x80);
            val_prev = val;
        }
    }
    write_reg(1, 0x300004, 0xC0);
    write_reg(2, 0x8, 0x0);
    printf("   FPGA bitstream loaded.\n");

    /* Zero DSP RAM */
    printf("2. Zeroing DSP RAM (BAR0, 0x40000 words)...\n");
    for (uint32_t i = 0; i < 0x10000; i++) {
        write_reg(0, i * 4, 0);
    }
    usleep(10000);

    /* Upload DSP program */
    printf("3. Uploading DSP program...\n");
#include "golden_dsp.c"
    printf("   Applying DSP patches...\n");
#include "golden_dsp_patch.c"
    printf("   DSP program loaded.\n");

    /* Get DMA address */
    uint32_t dma_addr = 0;
    if (ioctl(g_fd, MOTU_IOC_GET_DMA, &dma_addr) < 0) {
        perror("ioctl GET_DMA");
    } else {
        printf("   DMA physical address: 0x%X\n", dma_addr);
    }

    /* Kick-start DSP */
    printf("4. Booting DSP...\n");
    write_reg(2, 0x0, 0x0);
    write_reg(0, 0x3fffc, 0x0);
    write_reg(2, 0x4, 0x2);
    usleep(500000);

    printf("   Boot vector: 0x%X\n", read_reg(0, 0x3fffc));
    printf("   Port 1 status: 0x%X\n", read_reg(2, 0x0));
    printf("   Port 2 status: 0x%X\n", read_reg(2, 0x4));
    printf("\nDone. Watch the sync light.\n");

    free(fw);
}

static void usage(void) {
    fprintf(stderr, "Usage: poke <command> [args]\n\n");
    fprintf(stderr, "Commands:\n");
    fprintf(stderr, "  read    <bar> <offset>              Read a register\n");
    fprintf(stderr, "  write   <bar> <offset> <value>      Write a register\n");
    fprintf(stderr, "  scan    <bar> [start] [end]          Scan for non-zero registers\n");
    fprintf(stderr, "  monitor <bar> <offset> [count]       Monitor a register\n");
    fprintf(stderr, "  fpga    <firmware.rbf>               Load FPGA + DSP program\n");
    fprintf(stderr, "\nBar: 0=BAR0(DSP), 1=BAR1(regs), 2=BAR2(FPGA port)\n");
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        usage();
        return 1;
    }

    g_fd = open("/dev/motu_poke", O_RDWR);
    if (g_fd < 0) {
        perror("open /dev/motu_poke");
        fprintf(stderr, "Is the motu_poke_driver module loaded?\n");
        return 1;
    }

    if (strcmp(argv[1], "read") == 0) {
        cmd_read(argc, argv);
    } else if (strcmp(argv[1], "write") == 0) {
        cmd_write(argc, argv);
    } else if (strcmp(argv[1], "scan") == 0) {
        cmd_scan(argc, argv);
    } else if (strcmp(argv[1], "monitor") == 0) {
        cmd_monitor(argc, argv);
    } else if (strcmp(argv[1], "fpga") == 0) {
        cmd_fpga(argc, argv);
    } else {
        fprintf(stderr, "Unknown command: %s\n\n", argv[1]);
        usage();
        close(g_fd);
        return 1;
    }

    close(g_fd);
    return 0;
}
