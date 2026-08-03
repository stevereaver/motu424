// SPDX-License-Identifier: GPL-2.0
/*
 * motu424_init.c - Golden-sequence replay engine with DMA address
 *                  translation, DSP RAM zeroing, DSP kick-start, and
 *                  sync polling.
 *
 * Shared across Linux and Windows. Uses the PAL for all OS-specific
 * operations.
 *
 * The init sequence is a compact binary blob of 9-byte entries, each
 * representing a register write (bar, offset, value). The replay engine
 * writes each entry to the appropriate BAR, translating hardcoded
 * Windows DMA addresses to our own DMA buffer address.
 *
 * Full initialization order:
 *   1. Zero DSP RAM (clear stale state)
 *   2. Replay golden sequence (DSP program + patches + register config)
 *   3. DSP kick-start (boot vector + start signal)
 *   4. Post-init port configuration loop (4-port DMA setup)
 *   5. Sync polling (wait for clock lock)
 */

#include "motu424_pal.h"
#include "motu424_hw.h"

#include <string.h>

/* ---- DMA address translation -------------------------------------------- */

static uint32_t motu424_translate_dma(struct motu424_ctx *ctx, uint32_t val)
{
	uint32_t dma32;

	if (!ctx->dma_buf)
		return val;

	dma32 = (uint32_t)ctx->dma_addr;

	/* Magic address 1: 0x10914xxx -> our DMA base (page-aligned) */
	if ((val & WIN_DMA_MAGIC_1_MASK) == WIN_DMA_MAGIC_1_BASE)
		return (dma32 & WIN_DMA_MAGIC_1_MASK) |
			(val & ~WIN_DMA_MAGIC_1_MASK);

	/* Magic address 2: 0xFE870000 -> our DMA base */
	if (val == WIN_DMA_MAGIC_2)
		return dma32;

	/* Magic address 3: 0x90000000 -> our DMA base */
	if (val == WIN_DMA_MAGIC_3)
		return dma32;

	/* Magic address 4: 0xBFD70xxx -> our DMA base + offset */
	if ((val & WIN_DMA_MAGIC_4_MASK) == WIN_DMA_MAGIC_4_BASE)
		return (dma32 & WIN_DMA_MAGIC_4_MASK) |
			(val & ~WIN_DMA_MAGIC_4_MASK);

	/* Magic address 5: 0xBFF92xxx -> our DMA base + 0x222000 + offset */
	if ((val & WIN_DMA_MAGIC_5_MASK) == WIN_DMA_MAGIC_5_BASE)
		return dma32 + WIN_DMA_MAGIC_5_OFF +
			(val & ~WIN_DMA_MAGIC_5_MASK);

	return val;
}

/* ---- Poll register ------------------------------------------------------ */

int motu424_poll_reg(struct motu424_ctx *ctx, uint8_t bar, uint32_t offset,
		     uint32_t expected, int timeout_ms)
{
	uint32_t val;
	int i;

	for (i = 0; i < timeout_ms; i++) {
		val = motu_read(ctx, bar, offset);
		if (val == expected)
			return 0;
		pal_msleep(1);
	}

	pal_log(PAL_LOG_DBG, "poll timeout: bar=%u offset=0x%x "
		"expected=0x%x got=0x%x (after %d ms)\n",
		bar, offset, expected, val, timeout_ms);
	return -1;
}

/* ---- Zero DSP RAM ------------------------------------------------------- */

static void motu424_zero_dsp_ram(struct motu424_ctx *ctx)
{
	uint32_t i;

	pal_log(PAL_LOG_INFO, "Zeroing DSP RAM (64K words)\n");

	for (i = 0; i < 0x10000; i++) {
		pal_write32(ctx->iobase_dsp, i * 4, 0);

		/* Yield periodically */
		if ((i & 0xFFF) == 0)
			pal_cond_resched();
	}
}

/* ---- DSP kick-start ----------------------------------------------------- */

static void motu424_dsp_kickstart(struct motu424_ctx *ctx)
{
	pal_log(PAL_LOG_INFO, "Kick-starting DSP\n");

	/* Reset DSP */
	pal_write32(ctx->iobase_port, 0x0, 0x0);

	/* Clear boot vector */
	pal_write32(ctx->iobase_dsp, 0x3fffc, 0x0);

	/* Start DSP running */
	pal_write32(ctx->iobase_port, 0x4, 0x2);

	/* Wait for DSP to boot */
	pal_msleep(500);

	pal_log(PAL_LOG_INFO, "DSP boot vector: 0x%08x\n",
		pal_read32(ctx->iobase_dsp, 0x3fffc));
	pal_log(PAL_LOG_INFO, "Port 1 status: 0x%08x\n",
		pal_read32(ctx->iobase_port, 0x0));
	pal_log(PAL_LOG_INFO, "Port 2 status: 0x%08x\n",
		pal_read32(ctx->iobase_port, 0x4));
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
 * Periodically (every ~8 port iterations), it also checks:
 *   - BAR0 0x70d4 (status, expect non-zero, then clear)
 *   - BAR0 0x70d8 (counter)
 *   - BAR0 0x70dc (status)
 *
 * The 4 DMA addresses cycle: 0x...0000, 0x...0600, 0x...0c00, 0x...1200
 * These represent the 4 audio ports of the 24I/O.
 */

static void motu424_post_init_port_loop(struct motu424_ctx *ctx)
{
	int port_iter;
	int status_check_count = 0;

	pal_log(PAL_LOG_INFO, "Running post-init port configuration loop\n");

	/* Run enough iterations to configure all 4 ports.
	 * The Windows trace shows this loop running continuously; we run
	 * a bounded number of iterations to configure the ports. */
	for (port_iter = 0; port_iter < 32; port_iter++) {
		uint32_t dma_addr;

		/* Read current DMA address from BAR0 0x6fd8 */
		dma_addr = pal_read32(ctx->iobase_dsp, 0x6fd8);

		/* Write BAR0 0x7040 = 0x0 */
		pal_write32(ctx->iobase_dsp, 0x7040, 0x0);

		/* Read and clear BAR0 0x6fec */
		(void)pal_read32(ctx->iobase_dsp, 0x6fec);
		pal_write32(ctx->iobase_dsp, 0x6fec, 0x0);

		/* Check BAR2 status (should be 0x13 = configured + locked) */
		{
			uint32_t status = pal_read32(ctx->iobase_port, 0x0);
			if (status != MOTU_PORT_LOCK_STATUS) {
				pal_log(PAL_LOG_DBG,
					"port loop iter %d: status=0x%02x "
					"(expected 0x%02x)\n",
					port_iter, status,
					MOTU_PORT_LOCK_STATUS);
			}
		}

		/* Write BAR1 0x400000 = 0x10 (port strobe) */
		pal_write32(ctx->iobase_reg, 0x400000, 0x10);

		/* Periodic status check (every 8 iterations) */
		if ((port_iter & 7) == 7) {
			uint32_t s1, s2, s3;

			s1 = pal_read32(ctx->iobase_dsp, 0x70d4);
			if (s1)
				pal_write32(ctx->iobase_dsp, 0x70d4, 0x0);

			s2 = pal_read32(ctx->iobase_dsp, 0x70dc);
			s3 = pal_read32(ctx->iobase_dsp, 0x70d8);
			if (s3)
				pal_write32(ctx->iobase_dsp, 0x70d8, 0x0);
			if (s2)
				pal_write32(ctx->iobase_dsp, 0x70dc, 0x0);

			status_check_count++;
			pal_log(PAL_LOG_DBG,
				"port loop status check %d: "
				"0x70d4=0x%x 0x70d8=0x%x 0x70dc=0x%x\n",
				status_check_count, s1, s3, s2);
		}

		pal_cond_resched();
	}

	pal_log(PAL_LOG_INFO, "Port configuration loop complete\n");
}

/* ---- Replay sequence ---------------------------------------------------- */

int motu424_replay_sequence(struct motu424_ctx *ctx, const void *seq_data,
			   size_t seq_size)
{
	const uint8_t *blob = (const uint8_t *)seq_data;
	size_t count = seq_size / 9;
	size_t i;

	if (!seq_data || seq_size == 0 || (seq_size % 9) != 0) {
		pal_log(PAL_LOG_ERR, "invalid init sequence (%zu bytes)\n",
			seq_size);
		return -1;
	}

	pal_log(PAL_LOG_INFO, "Replaying %zu register writes\n", count);

	for (i = 0; i < count; i++) {
		const uint8_t *entry = blob + (i * 9);
		uint8_t bar = entry[0];
		uint32_t offset = (uint32_t)entry[1] |
				  ((uint32_t)entry[2] << 8) |
				  ((uint32_t)entry[3] << 16) |
				  ((uint32_t)entry[4] << 24);
		uint32_t value = (uint32_t)entry[5] |
				 ((uint32_t)entry[6] << 8) |
				 ((uint32_t)entry[7] << 16) |
				 ((uint32_t)entry[8] << 24);

		/* Translate DMA addresses */
		value = motu424_translate_dma(ctx, value);

		/* Handle poll entries */
		if (bar == MOTU_INIT_POLL_MARKER) {
			motu424_poll_reg(ctx, PAL_BAR_DSP, offset, value, 1000);
			continue;
		}

		motu_write(ctx, bar, offset, value);

		if ((i & 0x3FF) == 0)
			pal_cond_resched();
	}

	pal_log(PAL_LOG_INFO, "Init sequence replay complete (%zu writes)\n",
		count);
	return 0;
}

/* ---- Full hardware initialization ---------------------------------------- */

int motu424_hw_init(struct motu424_ctx *ctx)
{
	uint32_t status;
	int err;

	/* 1. Zero DSP RAM to clear stale state */
	motu424_zero_dsp_ram(ctx);

	/* 2. Shadow the current port configuration */
	if (ctx->iobase_reg)
		ctx->port_conf_shadow =
			pal_read32(ctx->iobase_reg, MOTU_REG_PORT_CONF);
	else
		ctx->port_conf_shadow = 0;

	/* 3. Poll sync status with 2-second timeout */
	err = motu424_poll_reg(ctx, PAL_BAR_DSP, MOTU_SYNC_REG_STATUS,
			       0x0, 2000);
	if (err)
		pal_log(PAL_LOG_WARN,
			"sync status poll failed (continuing)\n");

	/* 4. Check FPGA/clock lock status via BAR2 */
	status = pal_read32(ctx->iobase_port, 0x00);
	pal_log(PAL_LOG_INFO, "post-init port status: 0x%08x\n", status);

	if ((status & MOTU_PORT_LOCK_STATUS) == MOTU_PORT_LOCK_STATUS) {
		pal_log(PAL_LOG_INFO, "card sync achieved\n");
	} else {
		pal_log(PAL_LOG_WARN,
			"card not fully synced (status=0x%08x, "
			"expected mask 0x%02x)\n",
			status, MOTU_PORT_LOCK_STATUS);
	}

	return 0;
}

void motu424_hw_stop(struct motu424_ctx *ctx)
{
	uint32_t val;

	if (!ctx->iobase_reg)
		return;

	val = pal_read32(ctx->iobase_reg, MOTU_REG_PORT_CONF);
	val &= ~(MOTU_PORT_CONF_DMA_EN | MOTU_PORT_CONF_RUN |
		 MOTU_PORT_CONF_DMA_RD | MOTU_PORT_CONF_START);
	pal_write32(ctx->iobase_reg, MOTU_REG_PORT_CONF, val);
	ctx->port_conf_shadow = val;
	ctx->dma_running = 0;
}

/* ---- Full initialization sequence (called by platform frontend) ---------- */
/*
 * This function performs the complete initialization:
 *   1. Load FPGA bitstream (if firmware provided)
 *   2. Zero DSP RAM
 *   3. Replay golden sequence (DSP program + patches + register config)
 *   4. DSP kick-start
 *   5. Post-init port configuration loop
 *   6. Sync polling
 *
 * The platform frontend (Linux ALSA or Windows WDF) calls this instead
 * of calling the individual steps.
 */

int motu424_full_init(struct motu424_ctx *ctx,
		      const void *fpga_fw, size_t fpga_fw_size,
		      const void *init_seq, size_t init_seq_size)
{
	int err;

	/* 1. Load FPGA bitstream (cold boot) */
	if (fpga_fw && fpga_fw_size > 0) {
		err = motu424_load_fpga(ctx, fpga_fw, fpga_fw_size);
		if (err) {
			pal_log(PAL_LOG_ERR, "FPGA load failed\n");
			return err;
		}
	} else {
		pal_log(PAL_LOG_INFO,
			"No FPGA firmware — assuming warm boot\n");
	}

	/* 2. Zero DSP RAM */
	motu424_zero_dsp_ram(ctx);

	/* 3. Replay golden init sequence */
	if (init_seq && init_seq_size > 0) {
		err = motu424_replay_sequence(ctx, init_seq, init_seq_size);
		if (err) {
			pal_log(PAL_LOG_ERR, "Init replay failed\n");
			return err;
		}
	} else {
		pal_log(PAL_LOG_WARN,
			"No init sequence — skipping (warm boot only)\n");
	}

	/* 4. DSP kick-start */
	motu424_dsp_kickstart(ctx);

	/* 5. Post-init port configuration loop */
	motu424_post_init_port_loop(ctx);

	/* 6. Final sync check */
	return motu424_hw_init(ctx);
}
