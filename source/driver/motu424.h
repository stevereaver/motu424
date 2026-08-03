/* SPDX-License-Identifier: GPL-2.0 */
/*
 * motu424.h - Shared definitions for the MOTU PCI-424 ALSA driver
 *
 * Hardware: Mark of the Unicorn PCI-424 audio interface
 *   Vendor ID: 0x137a  Device ID: 0x0004
 *
 * PCI BAR layout (from lspci):
 *   BAR0:  4 MB prefetchable memory  - DSP program/data memory
 *   BAR1:  8 MB non-prefetchable memory - Control registers
 *   BAR2: 16 bytes I/O port           - FPGA configuration interface
 */

#ifndef _MOTU424_H
#define _MOTU424_H

#include <linux/types.h>
#include <linux/io.h>
#include <linux/spinlock.h>
#include <linux/pci.h>
#include <sound/core.h>
#include <sound/pcm.h>

/* ---- PCI identification ------------------------------------------------- */

#define MOTU_VENDOR_ID			0x137a
#define MOTU_DEVICE_ID			0x0004
#define MOTU_DRIVER_NAME		"snd_motu424"

/* ---- BAR indices ------------------------------------------------------- */

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

#define MOTU_PORT_CONF_DMA_EN		BIT(7)		/* 0x80   */
#define MOTU_PORT_CONF_RUN		BIT(17)		/* 0x20000  */
#define MOTU_PORT_CONF_DMA_RD		BIT(18)		/* 0x40000  */
#define MOTU_PORT_CONF_FPGA_RESET	BIT(16)		/* 0x10000  */
#define MOTU_PORT_CONF_START		BIT(31)		/* 0x80000000 */

/* ---- Interrupt bits ---------------------------------------------------- */

#define MOTU_INT_PERIOD_ELAPSED		BIT(0)

/* ---- Audio stream parameters ------------------------------------------- */
/*
 * The MOTU PCI-424 uses a multiplexed audio frame format. Each sample
 * period the card transfers a fixed-size frame containing all channels
 * for all ports. Empirically determined from the Windows driver trace:
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
	__u8	bar;
	__le32	offset;
	__le32	value;
} __packed;

#define MOTU_INIT_POLL_MARKER		0xFF

/* Poll entry: bar=0xFF, offset=target reg, value=expected value.
 * The driver reads the register repeatedly until it matches.
 */

/* ---- Firmware names ---------------------------------------------------- */

#define MOTU_FW_FPGA			"motu424/altera424b.rbf"
#define MOTU_FW_INIT_SEQ		"motu424/init_sequence.bin"

/* ---- Driver state structure -------------------------------------------- */

struct motu424 {
	struct pci_dev		*pci;
	struct snd_card		*card;
	struct snd_pcm		*pcm;

	/* BAR mappings */
	void __iomem		*iobase_dsp;	/* BAR0 - DSP memory     */
	void __iomem		*iobase_reg;	/* BAR1 - control regs   */
	void __iomem		*iobase_port;	/* BAR2 - FPGA config port */

	int			irq;

	/* DMA */
	void			*dma_buf;	/* coherent DMA buffer   */
	dma_addr_t		dma_addr;	/* physical address      */
	size_t			dma_size;	/* buffer size in bytes  */

	/* Playback state */
	struct snd_pcm_substream	*playback_substream;
	struct snd_pcm_substream	*capture_substream;
	unsigned int		dma_running;
	u64			start_time_ns;
	spinlock_t		lock;

	/* Hardware state */
	u32			port_conf_shadow;
	unsigned int		rate;
};

/* ---- Function prototypes (per module) ---------------------------------- */

/* motu424_main.c */
int motu424_create_card(struct pci_dev *pci, struct snd_card **card_out);
void motu424_destroy_card(struct snd_card *card);

/* motu424_fpga.c */
int motu424_load_fpga(struct motu424 *motu, const struct firmware *fw);

/* motu424_init.c */
int motu424_hw_init(struct motu424 *motu);
void motu424_hw_stop(struct motu424 *motu);
int motu424_replay_sequence(struct motu424 *motu, const struct firmware *seq);
int motu424_poll_reg(struct motu424 *motu, u8 bar, u32 offset, u32 expected,
		     int timeout_ms);

/* Full init: FPGA load + DSP zero + replay + kick-start + port loop + sync */
int motu424_full_init(struct motu424 *motu, const struct firmware *fpga_fw,
		      const struct firmware *init_seq);

/* motu424_dma.c */
int motu424_dma_alloc(struct motu424 *motu, size_t size);
void motu424_dma_free(struct motu424 *motu);
int motu424_dma_setup_sg(struct motu424 *motu);

/* motu424_pcm.c */
int motu424_pcm_new(struct motu424 *motu);

/* ---- Inline MMIO helpers ----------------------------------------------- */

static inline void motu_write(struct motu424 *motu, u8 bar, u32 offset,
			       u32 value)
{
	switch (bar) {
	case MOTU_BAR_DSP:
		iowrite32(value, motu->iobase_dsp + offset);
		break;
	case MOTU_BAR_REG:
		iowrite32(value, motu->iobase_reg + offset);
		break;
	case MOTU_BAR_PORT:
		iowrite32(value, motu->iobase_port + offset);
		break;
	}
}

static inline u32 motu_read(struct motu424 *motu, u8 bar, u32 offset)
{
	switch (bar) {
	case MOTU_BAR_DSP:
		return ioread32(motu->iobase_dsp + offset);
	case MOTU_BAR_REG:
		return ioread32(motu->iobase_reg + offset);
	case MOTU_BAR_PORT:
		return ioread32(motu->iobase_port + offset);
	default:
		return 0xffffffff;
	}
}

#endif /* _MOTU424_H */
