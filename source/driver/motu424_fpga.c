// SPDX-License-Identifier: GPL-2.0
/*
 * motu424_fpga.c - FPGA bitstream loading via bit-banged serial interface
 *
 * The MOTU PCI-424 card uses an Altera FPGA that must be configured on
 * every power-up. The configuration data (a .rbf raw binary file) is
 * clocked into the FPGA one bit at a time through a control register
 * in BAR1.
 *
 * Bit-bang protocol on BAR1 register 0x300008:
 *   Bit 6 (0x40) = FPGA configuration clock (DCLK)
 *   Bit 5 (0x20) = FPGA configuration data  (DATA0)
 *   Bit 7 (0x80) = part of the clock-high pattern (combined with 0x40)
 *
 * Sequence per bit (LSB first within each byte):
 *   1. Write 0x40 | data_bit  (clock LOW,  data on DATA0)
 *   2. Write 0xC0 | data_bit  (clock HIGH, data on DATA0)
 *   3. Write 0x40 | data_bit  (clock LOW,  data held — return to idle)
 *
 * Pre-configuration setup:
 *   - BAR2:0x04 = 0x01   (enable FPGA configuration mode)
 *   - BAR1:0x300000 = 0xE0
 *   - BAR1:0x300004 = 0xE0
 *   - BAR1:0x300008 = 0x00 (reset clock line)
 *
 * Post-configuration:
 *   - BAR1:0x300004 = 0xC0 (signal configuration complete)
 *   - BAR2:0x08 = 0x00
 */

#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/pci.h>
#include <linux/io.h>
#include <linux/sched.h>

#include "motu424.h"

/* FPGA bitbang register bit definitions */
#define FPGA_DCLK		0x40	/* bit 6: clock */
#define FPGA_DATA0		0x20	/* bit 5: data  */
#define FPGA_CLK_HIGH		(FPGA_DCLK | 0x80)
#define FPGA_CLK_LOW		FPGA_DCLK

/* FPGA control registers in BAR1 */
#define FPGA_REG_CTRL0		0x300000
#define FPGA_REG_CTRL1		0x300004
#define FPGA_REG_DATA		0x300008

/* BAR2 port registers */
#define FPGA_PORT_MODE		0x04
#define FPGA_PORT_DONE		0x08

#define FPGA_MODE_CONFIG	0x01

static void motu424_fpga_bitbang(struct motu424 *motu, u8 byte)
{
	int bit;

	for (bit = 0; bit < 8; bit++) {
		u32 data_bit = (byte & BIT(bit)) ? FPGA_DATA0 : 0;
		u32 val_low = FPGA_CLK_LOW | data_bit;
		u32 val_high = FPGA_CLK_HIGH | data_bit;

		/* Clock low with data */
		iowrite32(val_low, motu->iobase_reg + FPGA_REG_DATA);
		/* Clock high with data (FPGA latches on rising edge) */
		iowrite32(val_high, motu->iobase_reg + FPGA_REG_DATA);
		/* Return clock to low */
		iowrite32(val_low, motu->iobase_reg + FPGA_REG_DATA);
	}
}

int motu424_load_fpga(struct motu424 *motu, const struct firmware *fw)
{
	size_t i;
	int err = 0;

	if (!fw || !fw->data || fw->size == 0)
		return -EINVAL;

	dev_info(&motu->pci->dev,
		 "loading FPGA bitstream (%zu bytes)\n", fw->size);

	/* Pre-configuration: enable config mode on the card */
	iowrite32(FPGA_MODE_CONFIG, motu->iobase_port + FPGA_PORT_MODE);
	iowrite32(0xE0, motu->iobase_reg + FPGA_REG_CTRL0);
	iowrite32(0xE0, motu->iobase_reg + FPGA_REG_CTRL1);
	iowrite32(0x00, motu->iobase_reg + FPGA_REG_DATA);
	udelay(10);

	/* Bit-bang the entire bitstream, LSB first, one bit at a time */
	for (i = 0; i < fw->size; i++) {
		motu424_fpga_bitbang(motu, fw->data[i]);

		/* Yield periodically to avoid hogging the CPU */
		if ((i & 0x3FFF) == 0)
			cond_resched();
	}

	/* Post-configuration: signal completion */
	iowrite32(0xC0, motu->iobase_reg + FPGA_REG_CTRL1);
	iowrite32(0x00, motu->iobase_port + FPGA_PORT_DONE);
	msleep(50);

	/* Verify: read the FPGA status from BAR2 port 0x00.
	 * A value of 0x13 indicates the FPGA is configured and locked. */
	{
		u32 status = ioread32(motu->iobase_port + 0x00);
		dev_info(&motu->pci->dev, "FPGA status after load: 0x%08x\n",
			 status);
		if ((status & 0x13) != 0x13) {
			dev_warn(&motu->pci->dev,
				 "FPGA may not be fully configured "
				 "(status=0x%08x, expected 0x13)\n", status);
			/* Continue anyway — some cards report different
			 * status bits depending on attached I/O boxes */
		}
	}

	dev_info(&motu->pci->dev, "FPGA bitstream loaded\n");
	return err;
}
