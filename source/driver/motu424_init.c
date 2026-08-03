// SPDX-License-Identifier: GPL-2.0
/*
 * motu424_init.c - Hardware initialization and golden-sequence replay engine
 *
 * This module replays the register-write sequence captured from the original
 * Windows driver via QEMU VFIO PCI passthrough tracing. The sequence is
 * stored as a compact binary blob (see motu424.h for the entry format) and
 * loaded through the firmware subsystem.
 *
 * The replay engine also handles:
 *   - DMA address translation: certain values in the golden sequence are
 *     physical addresses of the Windows driver's DMA buffers. These must be
 *     patched to point to our own DMA buffer before writing to the hardware.
 *   - Post-replay polling: after the write sequence, the driver polls
 *     status registers until the card reports sync/lock.
 *
 * The golden sequence was captured by running the official MOTU Windows
 * driver inside a QEMU/KVM VM with the PCI card passed through via VFIO,
 * with QEMU tracing enabled on the MOTU PCI device's BAR regions.
 */

#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/io.h>
#include <linux/pci.h>
#include <linux/sched.h>
#include <linux/slab.h>

#include "motu424.h"

/* ---- DMA address translation -------------------------------------------- */
/*
 * The Windows driver hardcoded several physical addresses for its DMA
 * buffers. When replaying the captured sequence on Linux, we must replace
 * these with our own DMA buffer physical address.
 *
 * These magic values were identified by comparing the QEMU VFIO trace
 * of the Windows driver against the physical memory layout. The Windows
 * driver allocated DMA buffers at these addresses; we substitute our
 * own dma_addr. See linux/smart_replay.c translate() for the original
 * analysis.
 *
 *   0x10914xxx -> our DMA base (page-aligned portion)
 *   0xfe870000 -> our DMA base
 *   0x90000000 -> our DMA base
 *   0xbfd70xxx -> our DMA base + offset
 *   0xbff92xxx -> our DMA base + 0x222000 + offset
 */

/* Magic physical addresses from the Windows driver's init sequence */
#define WIN_DMA_MAGIC_1_BASE	0x10914000U
#define WIN_DMA_MAGIC_1_MASK	0xFFFFF000U
#define WIN_DMA_MAGIC_2		0xFE870000U
#define WIN_DMA_MAGIC_3		0x90000000U
#define WIN_DMA_MAGIC_4_BASE	0xBFD70000U
#define WIN_DMA_MAGIC_4_MASK	0xFFFF0000U
#define WIN_DMA_MAGIC_5_BASE	0xBFF92000U
#define WIN_DMA_MAGIC_5_MASK	0xFFFFF000U
#define WIN_DMA_MAGIC_5_OFF	0x222000U

static u32 motu424_translate_dma(struct motu424 *motu, u32 val)
{
	dma_addr_t dma = motu->dma_addr;
	u32 dma32 = (u32)dma;

	if (!dma)
		return val;

	if ((val & WIN_DMA_MAGIC_1_MASK) == WIN_DMA_MAGIC_1_BASE)
		return dma32 | (val & ~WIN_DMA_MAGIC_1_MASK);

	if (val == WIN_DMA_MAGIC_2)
		return dma32;

	if (val == WIN_DMA_MAGIC_3)
		return dma32;

	if ((val & WIN_DMA_MAGIC_4_MASK) == WIN_DMA_MAGIC_4_BASE)
		return dma32 | (val & ~WIN_DMA_MAGIC_4_MASK);

	if ((val & WIN_DMA_MAGIC_5_MASK) == WIN_DMA_MAGIC_5_BASE)
		return dma32 + WIN_DMA_MAGIC_5_OFF + (val & ~WIN_DMA_MAGIC_5_MASK);

	return val;
}

/* ---- Poll register for expected value ---------------------------------- */

int motu424_poll_reg(struct motu424 *motu, u8 bar, u32 offset, u32 expected,
		     int timeout_ms)
{
	int retry = 0;
	int max_retries = timeout_ms * 100; /* 10 us per retry */
	u32 val;

	do {
		val = motu_read(motu, bar, offset);
		if (val == expected)
			return 0;
		udelay(10);
	} while (retry++ < max_retries);

	dev_warn(&motu->pci->dev,
		 "poll timeout: bar=%d off=0x%x expected=0x%x got=0x%x\n",
		 bar, offset, expected, val);
	return -ETIMEDOUT;
}

/* ---- Replay the golden init sequence ----------------------------------- */

int motu424_replay_sequence(struct motu424 *motu, const struct firmware *seq)
{
	const struct motu_init_entry *entry;
	size_t count;
	size_t i;
	int err = 0;

	if (!seq || !seq->data || seq->size < sizeof(*entry))
		return -EINVAL;

	if (seq->size % sizeof(*entry) != 0) {
		dev_err(&motu->pci->dev,
			"init sequence size %zu not multiple of %zu\n",
			seq->size, sizeof(*entry));
		return -EINVAL;
	}

	count = seq->size / sizeof(*entry);
	entry = (const struct motu_init_entry *)seq->data;

	dev_info(&motu->pci->dev,
		 "replaying init sequence: %zu register writes\n", count);

	for (i = 0; i < count; i++) {
		u8 bar = entry[i].bar;
		u32 offset = le32_to_cpu(entry[i].offset);
		u32 value = le32_to_cpu(entry[i].value);

		/* Patch DMA physical addresses */
		value = motu424_translate_dma(motu, value);

		motu_write(motu, bar, offset, value);

		/* Yield periodically to avoid hogging the CPU */
		if ((i & 0x3FF) == 0)
			cond_resched();
	}

	dev_info(&motu->pci->dev, "init sequence replay complete\n");
	return err;
}

/* ---- DSP RAM zeroing ---------------------------------------------------- */
/*
 * Clear DSP program memory before loading the golden sequence.
 * Stale DSP state from a previous session can prevent the 24I/O from
 * initializing correctly.
 */

static void motu424_zero_dsp_ram(struct motu424 *motu)
{
	u32 i;

	dev_info(&motu->pci->dev, "zeroing DSP RAM (64K words)\n");

	for (i = 0; i < 0x10000; i++) {
		iowrite32(0, motu->iobase_dsp + (i * 4));

		if ((i & 0xFFF) == 0)
			cond_resched();
	}
}

/* ---- DSP kick-start ---------------------------------------------------- */
/*
 * After loading the DSP program, the DSP must be started by:
 *   1. Reset DSP (BAR2 0x0 = 0x0)
 *   2. Clear boot vector (BAR0 0x3fffc = 0x0)
 *   3. Start DSP running (BAR2 0x4 = 0x2)
 *   4. Wait 500ms for DSP to boot
 *
 * Without this, the DSP sits idle and never configures the audio ports,
 * so the 24I/O interface box never sees clock/sync.
 */

static void motu424_dsp_kickstart(struct motu424 *motu)
{
	dev_info(&motu->pci->dev, "kick-starting DSP\n");

	iowrite32(0x0, motu->iobase_port + 0x0);
	iowrite32(0x0, motu->iobase_dsp + 0x3fffc);
	iowrite32(0x2, motu->iobase_port + 0x4);

	msleep(500);

	dev_info(&motu->pci->dev, "DSP boot vector: 0x%08x\n",
		 ioread32(motu->iobase_dsp + 0x3fffc));
	dev_info(&motu->pci->dev, "Port 1 status: 0x%08x\n",
		 ioread32(motu->iobase_port + 0x0));
	dev_info(&motu->pci->dev, "Port 2 status: 0x%08x\n",
		 ioread32(motu->iobase_port + 0x4));
}

/* ---- Post-init port configuration loop ----------------------------------- */
/*
 * After the golden sequence and DSP kick-start, the Windows driver runs
 * a repeating loop that configures 4 audio ports. Each iteration:
 *   1. Read BAR0 0x6fd8 (current DMA address)
 *   2. Write BAR0 0x7040 = 0x0
 *   3. Read BAR0 0x6fec
 *   4. Write BAR0 0x6fec = 0x0
 *   5. Read BAR2 0x0 (status check, expect 0x13)
 *   6. Write BAR1 0x400000 = 0x10
 *
 * Periodically it also checks/clears status at 0x70d4/0x70d8/0x70dc.
 *
 * The 24I/O has 4 audio ports that need this configuration.
 */

static void motu424_post_init_port_loop(struct motu424 *motu)
{
	int iter;

	dev_info(&motu->pci->dev, "running post-init port configuration loop\n");

	for (iter = 0; iter < 32; iter++) {
		/* Read current DMA address */
		(void)ioread32(motu->iobase_dsp + 0x6fd8);

		/* Port strobe */
		iowrite32(0x0, motu->iobase_dsp + 0x7040);
		(void)ioread32(motu->iobase_dsp + 0x6fec);
		iowrite32(0x0, motu->iobase_dsp + 0x6fec);

		/* Check BAR2 status */
		{
			u32 status = ioread32(motu->iobase_port + 0x0);
			if (status != MOTU_PORT_LOCK_STATUS)
				dev_dbg(&motu->pci->dev,
					"port loop iter %d: status=0x%02x\n",
					iter, status);
		}

		/* Port strobe on BAR1 */
		iowrite32(0x10, motu->iobase_reg + 0x400000);

		/* Periodic status check (every 8 iterations) */
		if ((iter & 7) == 7) {
			u32 s;

			s = ioread32(motu->iobase_dsp + 0x70d4);
			if (s)
				iowrite32(0x0, motu->iobase_dsp + 0x70d4);

			s = ioread32(motu->iobase_dsp + 0x70dc);
			(void)ioread32(motu->iobase_dsp + 0x70d8);
			iowrite32(0x0, motu->iobase_dsp + 0x70d8);
			if (s)
				iowrite32(0x0, motu->iobase_dsp + 0x70dc);
		}

		cond_resched();
	}

	dev_info(&motu->pci->dev, "port configuration loop complete\n");
}

/* ---- Hardware initialization (post-replay) ------------------------------ */
/*
 * After the golden sequence is replayed, the card needs a few additional
 * steps to achieve full sync:
 *
 * 1. Poll the sync status registers until they indicate lock.
 * 2. Verify the FPGA configuration status via BAR2.
 *
 * The poll offsets and expected values were derived from the QEMU trace
 * (smart_replay.c) and the post-init register dump.
 */

/* Sync status registers in BAR0 (DSP memory) */
#define MOTU_SYNC_REG_STATUS	0x6fec
#define MOTU_SYNC_REG_CTRL	0x7040
#define MOTU_SYNC_REG_DSP	0x3fffc

int motu424_hw_init(struct motu424 *motu)
{
	u32 status;
	int err;

	/* 1. Zero DSP RAM to clear stale state */
	motu424_zero_dsp_ram(motu);

	/* 2. Shadow the current port configuration */
	if (motu->iobase_reg)
		motu->port_conf_shadow =
			ioread32(motu->iobase_reg + MOTU_REG_PORT_CONF);
	else
		motu->port_conf_shadow = 0;

	/* 3. Poll sync status registers with a generous timeout.
	 * The card may need up to 2 seconds to achieve lock after the
	 * init sequence, depending on attached I/O boxes. */
	err = motu424_poll_reg(motu, MOTU_BAR_DSP, MOTU_SYNC_REG_STATUS,
			       0x0, 2000);
	if (err)
		dev_warn(&motu->pci->dev, "sync status poll failed (continuing)\n");

	/* 4. Check the FPGA/clock lock status via BAR2.
	 * A value of 0x13 indicates FPGA configured + clock locked. */
	status = ioread32(motu->iobase_port + 0x00);
	dev_info(&motu->pci->dev, "post-init port status: 0x%08x\n", status);

	if ((status & MOTU_PORT_LOCK_STATUS) == MOTU_PORT_LOCK_STATUS) {
		dev_info(&motu->pci->dev, "card sync achieved\n");
	} else {
		dev_warn(&motu->pci->dev,
			 "card not fully synced (status=0x%08x, "
			 "expected mask 0x%02x)\n",
			 status, MOTU_PORT_LOCK_STATUS);
		/* Continue anyway — the card may still be usable */
	}

	return 0;
}

/* ---- Full initialization (called by main driver) ------------------------- */

int motu424_full_init(struct motu424 *motu, const struct firmware *fpga_fw,
		      const struct firmware *init_seq)
{
	int err;

	/* 1. Load FPGA bitstream (cold boot) */
	if (fpga_fw) {
		err = motu424_load_fpga(motu, fpga_fw);
		if (err) {
			dev_err(&motu->pci->dev, "FPGA load failed\n");
			return err;
		}
	}

	/* 2. Zero DSP RAM */
	motu424_zero_dsp_ram(motu);

	/* 3. Replay golden init sequence */
	if (init_seq) {
		err = motu424_replay_sequence(motu, init_seq);
		if (err) {
			dev_err(&motu->pci->dev, "Init replay failed\n");
			return err;
		}
	}

	/* 4. DSP kick-start */
	motu424_dsp_kickstart(motu);

	/* 5. Post-init port configuration loop */
	motu424_post_init_port_loop(motu);

	/* 6. Final sync check */
	return motu424_hw_init(motu);
}

void motu424_hw_stop(struct motu424 *motu)
{
	u32 val;

	if (!motu->iobase_reg)
		return;

	/* Stop DMA and mask interrupts */
	val = ioread32(motu->iobase_reg + MOTU_REG_PORT_CONF);
	val &= ~(MOTU_PORT_CONF_DMA_EN | MOTU_PORT_CONF_RUN |
		 MOTU_PORT_CONF_START);
	iowrite32(val, motu->iobase_reg + MOTU_REG_PORT_CONF);
	motu->port_conf_shadow = val;

	iowrite32(0, motu->iobase_reg + MOTU_REG_INT_MASK);
	motu->dma_running = 0;
}
