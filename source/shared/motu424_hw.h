/* SPDX-License-Identifier: GPL-2.0 */
/*
 * motu424_hw.h - Hardware definitions for the MOTU PCI-424 audio interface.
 *
 * OS-agnostic constants, register maps, and data format definitions.
 * Used by both Linux and Windows drivers via the PAL.
 *
 * Hardware: Mark of the Unicorn PCI-424
 *   Vendor ID: 0x137a  Device ID: 0x0004
 *
 * PCI BAR layout:
 *   BAR0:  4 MB prefetchable memory  - DSP program/data memory
 *   BAR1:  8 MB non-prefetchable memory - Control registers
 *   BAR2: 16 bytes I/O port           - FPGA configuration interface
 */

#ifndef _MOTU424_HW_H
#define _MOTU424_HW_H

/* MSVC compatibility for GCC attributes */
#ifdef _MSC_VER
#include <stdint.h>
#define __packed
#pragma pack(push, 1)
#else
#include <stdint.h>
#define __packed __attribute__((packed))
#endif

/* ---- PCI identification ------------------------------------------------- */

#define MOTU_VENDOR_ID			0x137a
#define MOTU_DEVICE_ID			0x0004
#define MOTU_DRIVER_NAME		"motu424"

/* ---- BAR indices (same as PAL_BAR_* in motu424_pal.h) ------------------- */

#define MOTU_BAR_DSP			0	/* 4 MB prefetchable - DSP memory  */
#define MOTU_BAR_REG			1	/* 8 MB non-prefetch - registers   */
#define MOTU_BAR_PORT			2	/* 16 bytes I/O - FPGA config port */

#define MOTU_BAR_DSP_SIZE		(4 * 1024 * 1024)
#define MOTU_BAR_REG_SIZE		(8 * 1024 * 1024)
#define MOTU_BAR_PORT_SIZE		16

/* ---- BAR1 register map (control registers) ----------------------------- */
/* Offsets discovered via QEMU VFIO trace analysis. */

#define MOTU_REG_PORT_CONF		0x00	/* port configuration / control    */
#define MOTU_REG_PORT_SIZE		0x04	/* port buffer size                */
#define MOTU_REG_UNKNOWN_08		0x08
#define MOTU_REG_CLOCK_COUNT		0x1c	/* clock counter                  */
#define MOTU_REG_DMA_BASE		0x18	/* DMA descriptor table base addr  */
#define MOTU_REG_DMA_SIZE		0x14	/* DMA transfer size               */
#define MOTU_REG_DMA_CTRL		0x10	/* DMA control                     */
#define MOTU_REG_INT_STATUS		0x20	/* interrupt status                */
#define MOTU_REG_INT_ACK		0x20	/* interrupt acknowledge (same)    */
#define MOTU_REG_INT_MASK		0x24	/* interrupt mask                   */

/* FPGA bitbang control register in BAR1 */
#define MOTU_REG_FPGA_CTRL		0x300008

/* ---- Port configuration bits (MOTU_REG_PORT_CONF) ----------------------- */

#define MOTU_PORT_CONF_DMA_EN		(1u << 7)	 /* 0x80      */
#define MOTU_PORT_CONF_FPGA_RESET	(1u << 16)	 /* 0x10000    */
#define MOTU_PORT_CONF_RUN		(1u << 17)	 /* 0x20000    */
#define MOTU_PORT_CONF_DMA_RD		(1u << 18)	 /* 0x40000    */
#define MOTU_PORT_CONF_START		(1u << 31)	 /* 0x80000000 */

/* ---- Interrupt bits ---------------------------------------------------- */

#define MOTU_INT_PERIOD_ELAPSED		(1u << 0)

/* ---- Sync status ------------------------------------------------------- */

#define MOTU_SYNC_REG_STATUS		0x70d0
#define MOTU_PORT_LOCK_STATUS		0x13

/* ---- Audio stream parameters ------------------------------------------- */
/*
 * The MOTU PCI-424 uses a multiplexed audio frame format. Each sample
 * period the card transfers a fixed-size frame containing all channels
 * for all ports:
 *
 *   98 channels * 4 bytes = 392 bytes per frame
 *
 * The first two 32-bit words of each frame are status/magic words.
 * Audio sample data begins at word offset 2.
 */

#define MOTU_CHANNELS_PER_FRAME		98
#define MOTU_BYTES_PER_FRAME		(MOTU_CHANNELS_PER_FRAME * 4)
#define MOTU_STATUS_WORDS		2

/* Supported sample rates */
#define MOTU_RATE_44100			44100
#define MOTU_RATE_48000			48000
#define MOTU_RATE_88200			88200
#define MOTU_RATE_96000			96000

/* ---- DMA buffer parameters --------------------------------------------- */

#define MOTU_DMA_BUF_MAX		(4 * 1024 * 1024)	/* 4 MB cap  */
#define MOTU_DMA_PERIODS_MIN		2
#define MOTU_DMA_PERIODS_MAX		16
#define MOTU_DMA_PERIOD_BYTES_MIN	1024
#define MOTU_DMA_PERIOD_BYTES_MAX	(64 * 1024)

/* ---- Initialization sequence entry -------------------------------------- */
/*
 * Compact representation of a single register write from the golden
 * QEMU trace. Used by the init-sequence replay engine.
 *
 * Each entry is 9 bytes:
 *   bar    : 1 byte  (0, 1, or 2)
 *   offset: 4 bytes  (little-endian)
 *   value : 4 bytes  (little-endian)
 *
 * A bar value of 0xFF marks a poll entry: the driver reads the register
 * and waits until it matches value (with optional timeout).
 */

struct motu_init_entry {
	uint8_t		bar;
	uint32_t	offset;	/* little-endian in the blob */
	uint32_t	value;	/* little-endian in the blob */
} __packed;

#define MOTU_INIT_POLL_MARKER		0xFF

/* ---- Firmware names ---------------------------------------------------- */

#define MOTU_FW_FPGA			"motu424/altera424b.rbf"
#define MOTU_FW_INIT_SEQ		"motu424/init_sequence.bin"

/* ---- Windows DMA magic address translation ----------------------------- */
/*
 * The Windows driver hardcoded several physical addresses for its DMA
 * buffers. When replaying the captured sequence, we must replace these
 * with our own DMA buffer physical address.
 *
 * From QEMU trace analysis (smart_replay.c translate() function):
 *   0x10914xxx -> our DMA base (page-aligned portion)
 *   0xfe870000 -> our DMA base
 *   0x90000000 -> our DMA base
 *   0xbfd70xxx -> our DMA base + offset
 *   0xbff92xxx -> our DMA base + 0x222000 + offset
 */

#define WIN_DMA_MAGIC_1_BASE	0x10914000U
#define WIN_DMA_MAGIC_1_MASK	0xFFFFF000U
#define WIN_DMA_MAGIC_2		0xFE870000U
#define WIN_DMA_MAGIC_3		0x90000000U
#define WIN_DMA_MAGIC_4_BASE	0xBFD70000U
#define WIN_DMA_MAGIC_4_MASK	0xFFFF0000U
#define WIN_DMA_MAGIC_5_BASE	0xBFF92000U
#define WIN_DMA_MAGIC_5_MASK	0xFFFFF000U
#define WIN_DMA_MAGIC_5_OFF	0x222000U

#ifdef _MSC_VER
#pragma pack(pop)
#endif

#endif /* _MOTU424_HW_H */
