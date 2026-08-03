// SPDX-License-Identifier: GPL-2.0
/*
 * motu424_dma.c - DMA buffer allocation and scatter-gather descriptor setup.
 *
 * Shared across Linux and Windows. Uses the PAL for coherent DMA
 * allocation and MMIO access.
 *
 * The card's DMA engine reads a descriptor table in DSP memory (BAR0)
 * containing the physical address and size of the audio data buffer.
 * We allocate a single contiguous coherent buffer and write its
 * address to the descriptor registers.
 */

#include "motu424_pal.h"
#include "motu424_hw.h"

#include <string.h>

/* Page size — 4096 on both Linux and Windows x86/x64 */
#define PAL_PAGE_SIZE	4096
#define PAL_PAGE_MASK	(~(PAL_PAGE_SIZE - 1))

static size_t page_align(size_t size)
{
	return (size + PAL_PAGE_SIZE - 1) & PAL_PAGE_MASK;
}

int motu424_dma_alloc(struct motu424_ctx *ctx, size_t size)
{
	int err;

	if (ctx->dma_buf) {
		if (ctx->dma_size >= size)
			return 0;
		motu424_dma_free(ctx);
	}

	size = page_align(size);
	if (size > MOTU_DMA_BUF_MAX)
		size = MOTU_DMA_BUF_MAX;

	err = pal_dma_alloc(ctx->dev, size, &ctx->dma_buf, &ctx->dma_addr);
	if (err) {
		pal_log(PAL_LOG_ERR,
			"failed to allocate %zu byte DMA buffer\n", size);
		return err;
	}

	ctx->dma_size = size;
	memset(ctx->dma_buf, 0, size);

	pal_log(PAL_LOG_INFO,
		"allocated %zu byte DMA buffer at phys 0x%llx\n",
		size, (unsigned long long)ctx->dma_addr);

	/* Set up the scatter-gather descriptor table in DSP memory */
	err = motu424_dma_setup_sg(ctx);
	if (err) {
		pal_dma_free(ctx->dev, ctx->dma_buf, ctx->dma_addr,
			     ctx->dma_size);
		ctx->dma_buf = NULL;
		return err;
	}

	return 0;
}

void motu424_dma_free(struct motu424_ctx *ctx)
{
	if (ctx->dma_buf) {
		pal_dma_free(ctx->dev, ctx->dma_buf, ctx->dma_addr,
			     ctx->dma_size);
		ctx->dma_buf = NULL;
		ctx->dma_addr = 0;
		ctx->dma_size = 0;
	}
}

int motu424_dma_setup_sg(struct motu424_ctx *ctx)
{
	uint32_t dma_addr32;
	uint32_t dma_size32;

	if (!ctx->dma_buf)
		return -1;

	dma_addr32 = (uint32_t)ctx->dma_addr;
	dma_size32 = (uint32_t)ctx->dma_size;

	/* Write the DMA descriptor into BAR0 DSP memory.
	 *
	 * Offsets 0x7008 and 0x70a4 are the DMA base address registers
	 * observed in the post_init trace. 0x30000 is the buffer config
	 * value (buffer size in samples), and 0x59c is the transfer
	 * configuration word.
	 */
	pal_write32(ctx->iobase_dsp, 0x7008, dma_addr32);
	pal_write32(ctx->iobase_dsp, 0x70a4, dma_addr32);
	pal_write32(ctx->iobase_dsp, 0x70a8, 0x30000); /* buffer config */
	pal_write32(ctx->iobase_dsp, 0x70ac, 0x59c);   /* transfer config */

	/* Also set the BAR1 DMA base register */
	pal_write32(ctx->iobase_reg, MOTU_REG_DMA_BASE, dma_addr32);

	pal_log(PAL_LOG_INFO, "DMA SG setup: addr=0x%08x size=%u\n",
		dma_addr32, dma_size32);

	return 0;
}
