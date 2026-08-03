// SPDX-License-Identifier: GPL-2.0
/*
 * motu424_dma.c - DMA buffer allocation and scatter-gather management
 *
 * The MOTU PCI-424 is a bus-mastering DMA device. It transfers audio data
 * to/from a contiguous physical memory buffer. The card's DMA engine reads
 * a descriptor table (located in DSP memory at BAR0) that contains physical
 * addresses and sizes of the audio data pages.
 *
 * For simplicity and reliability, this driver allocates a single large
 * coherent DMA buffer (up to 4 MB) rather than building a scatter-gather
 * list from individual pages. This matches the behavior observed in the
 * Windows driver trace, which used a single contiguous buffer.
 *
 * The physical address of this buffer is written to the card's DMA base
 * address register (BAR1 offset 0x18) during stream preparation.
 */

#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/pci.h>

#include "motu424.h"

int motu424_dma_alloc(struct motu424 *motu, size_t size)
{
	int err;

	if (motu->dma_buf) {
		/* Already allocated — check if it's large enough */
		if (motu->dma_size >= size)
			return 0;
		motu424_dma_free(motu);
	}

	/* Round up to page size */
	size = PAGE_ALIGN(size);
	if (size > MOTU_DMA_BUF_MAX)
		size = MOTU_DMA_BUF_MAX;

	motu->dma_buf = dma_alloc_coherent(&motu->pci->dev, size,
					   &motu->dma_addr, GFP_KERNEL);
	if (!motu->dma_buf) {
		dev_err(&motu->pci->dev, "failed to allocate %zu byte DMA buffer\n",
			size);
		return -ENOMEM;
	}

	motu->dma_size = size;
	memset(motu->dma_buf, 0, size);

	dev_info(&motu->pci->dev,
		 "allocated %zu byte DMA buffer at phys %pad\n",
		 size, &motu->dma_addr);

	/* Set up the scatter-gather descriptor table in DSP memory.
	 * The card expects a table of (address, size) pairs in BAR0.
	 * For a single contiguous buffer, we write one entry. */
	err = motu424_dma_setup_sg(motu);
	if (err) {
		dma_free_coherent(&motu->pci->dev, motu->dma_size,
				  motu->dma_buf, motu->dma_addr);
		motu->dma_buf = NULL;
		return err;
	}

	return 0;
}

void motu424_dma_free(struct motu424 *motu)
{
	if (motu->dma_buf) {
		dma_free_coherent(&motu->pci->dev, motu->dma_size,
				  motu->dma_buf, motu->dma_addr);
		motu->dma_buf = NULL;
		motu->dma_addr = 0;
		motu->dma_size = 0;
	}
}

int motu424_dma_setup_sg(struct motu424 *motu)
{
	u32 dma_addr32;
	u32 dma_size32;

	if (!motu->dma_buf)
		return -ENOMEM;

	dma_addr32 = (u32)motu->dma_addr;
	dma_size32 = (u32)motu->dma_size;

	/* Write the DMA descriptor into BAR0 DSP memory.
	 *
	 * The descriptor format (derived from the Windows driver trace)
	 * uses a simple (base_address, size) pair at fixed offsets in
	 * the DSP memory map. The card's DMA engine reads these on
	 * stream start.
	 *
	 * Offsets 0x7008 and 0x70a4 are the DMA base address registers
	 * observed in the post_init trace. 0x30000 is the buffer config
	 * value (buffer size in samples), and 0x59c is the transfer
	 * configuration word.
	 */
	iowrite32(dma_addr32, motu->iobase_dsp + 0x7008);
	iowrite32(dma_addr32, motu->iobase_dsp + 0x70a4);
	iowrite32(0x30000, motu->iobase_dsp + 0x70a8); /* buffer config */
	iowrite32(0x59c, motu->iobase_dsp + 0x70ac);   /* transfer config */

	/* Also set the BAR1 DMA base register */
	iowrite32(dma_addr32, motu->iobase_reg + MOTU_REG_DMA_BASE);

	dev_info(&motu->pci->dev,
		 "DMA SG setup: addr=0x%08x size=%u\n",
		 dma_addr32, dma_size32);

	return 0;
}
