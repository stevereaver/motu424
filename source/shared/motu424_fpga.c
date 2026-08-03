// SPDX-License-Identifier: GPL-2.0
/*
 * motu424_fpga.c - FPGA bitstream loading via bit-banged serial on BAR1.
 *
 * Shared across Linux and Windows. Uses the PAL for all OS-specific
 * operations (MMIO, timing, logging).
 *
 * The Altera FPGA bitstream is loaded by bit-banging serial data through
 * BAR1 register 0x300008. Bit 7 is the clock, bit 5 is the data line.
 *
 * Protocol derived from the QEMU VFIO trace (bitbang_head.txt) and the
 * working poke_fpga.c tool:
 *
 *   0x40 = clock low,  data = 0
 *   0x60 = clock low,  data = 1
 *   0xC0 = clock high, data = 0
 *   0xE0 = clock high, data = 1
 *
 * Each bit requires three writes to 0x300008:
 *   1. Write previous value (data setup)
 *   2. Write new value (data hold)
 *   3. Write value | 0x80 (clock pulse)
 */

#include "motu424_pal.h"
#include "motu424_hw.h"

/* FPGA bitbang register bits (BAR1 0x300008) */
#define FPGA_DATA_BIT		0x20	/* bit 5: serial data */
#define FPGA_CLK_BIT		0x80	/* bit 7: clock        */
#define FPGA_BASE_VAL		0x40	/* base value (bit 6 set) */

int motu424_load_fpga(struct motu424_ctx *ctx, const void *fw_data,
		      size_t fw_size)
{
	const uint8_t *bitstream = (const uint8_t *)fw_data;
	size_t i;
	int bit;
	uint32_t val_prev = FPGA_BASE_VAL;

	if (!fw_data || fw_size == 0)
		return -1;

	pal_log(PAL_LOG_INFO, "Loading FPGA bitstream (%zu bytes)\n", fw_size);

	/* 1. BAR2 reset — put FPGA into configuration mode */
	pal_write32(ctx->iobase_port, 0x4, 0x1);

	/* 2. Pre-configuration registers */
	pal_write32(ctx->iobase_reg, 0x300000, 0xE0);
	pal_write32(ctx->iobase_reg, 0x300004, 0xE0);
	pal_write32(ctx->iobase_reg, MOTU_REG_FPGA_CTRL, 0x00);

	/* 3. Bit-bang each bit of the bitstream (LSB first) */
	for (i = 0; i < fw_size; i++) {
		uint8_t byte = bitstream[i];

		for (bit = 0; bit < 8; bit++) {
			uint32_t data_bit = (byte & (1 << bit)) ?
				FPGA_DATA_BIT : 0;
			uint32_t val = FPGA_BASE_VAL | data_bit;

			/* Write previous, then current, then clock pulse */
			pal_write32(ctx->iobase_reg, MOTU_REG_FPGA_CTRL,
				    val_prev);
			pal_write32(ctx->iobase_reg, MOTU_REG_FPGA_CTRL, val);
			pal_write32(ctx->iobase_reg, MOTU_REG_FPGA_CTRL,
				    val | FPGA_CLK_BIT);

			val_prev = val;
		}

		/* Yield periodically to avoid hogging the CPU */
		if ((i & 0x3FFF) == 0)
			pal_cond_resched();
	}

	/* 4. Finalize FPGA configuration */
	pal_write32(ctx->iobase_reg, 0x300004, 0xC0);
	pal_write32(ctx->iobase_port, 0x8, 0x0);

	/* 5. Check FPGA status via BAR2 */
	{
		uint32_t status = pal_read32(ctx->iobase_port, 0x00);
		pal_log(PAL_LOG_INFO, "FPGA status after load: 0x%08x\n",
			status);

		if ((status & MOTU_PORT_LOCK_STATUS) !=
		    MOTU_PORT_LOCK_STATUS) {
			pal_log(PAL_LOG_WARN,
				"FPGA may not be fully configured "
				"(status=0x%08x, expected 0x%02x)\n",
				status, MOTU_PORT_LOCK_STATUS);
			/* Continue anyway — warm boot may have FPGA loaded */
		}
	}

	pal_log(PAL_LOG_INFO, "FPGA bitstream loaded (%zu bytes)\n", fw_size);
	return 0;
}
