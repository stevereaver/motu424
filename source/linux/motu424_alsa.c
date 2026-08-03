// SPDX-License-Identifier: GPL-2.0
/*
 * motu424_alsa.c - Linux ALSA frontend for the MOTU PCI-424 driver.
 *
 * Implements the Linux-specific PCI probe, ALSA card/PCM setup, and
 * PCM operations. Uses the shared core (via PAL) for all hardware
 * interaction.
 *
 * Build: make -C /lib/modules/$(uname -r)/build M=$(pwd) modules
 * Load:  insmod snd_motu424.ko
 */

#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <sound/core.h>
#include <sound/pcm.h>

#include "motu424_pal.h"
#include "motu424_hw.h"

/* ---- Module parameters -------------------------------------------------- */

static int index = SNDRV_DEFAULT_IDX1;
static char *id = SNDRV_DEFAULT_STR1;
module_param(index, int, 0444);
module_param(id, charp, 0444);
MODULE_AUTHOR("MOTU PCI-424 ALSA Driver Project");
MODULE_DESCRIPTION("MOTU PCI-424 ALSA driver");
MODULE_LICENSE("GPL v2");

/* ---- Linux device wrapper ------------------------------------------------ */

struct motu424_linux {
	struct pal_device	pal_dev;	/* must be first */
	struct pci_dev		*pci;
	struct snd_card		*card;
	struct motu424_ctx	ctx;
	const struct firmware	*fw_fpga;
	const struct firmware	*fw_init;

	/* ALSA substreams (for IRQ handler) */
	struct snd_pcm_substream	*playback_substream;
	struct snd_pcm_substream	*capture_substream;
};

/* ---- IRQ handler -------------------------------------------------------- */

static void motu424_irq_handler(void *ctx_data)
{
	struct motu424_linux *motu = ctx_data;
	u32 status;

	status = pal_read32(motu->ctx.iobase_reg, MOTU_REG_INT_STATUS);
	if (!(status & MOTU_INT_PERIOD_ELAPSED))
		return;

	/* Acknowledge */
	pal_write32(motu->ctx.iobase_reg, MOTU_REG_INT_ACK, status);

	if (motu->playback_substream)
		snd_pcm_period_elapsed(motu->playback_substream);
	if (motu->capture_substream)
		snd_pcm_period_elapsed(motu->capture_substream);
}

/* ---- PCM hardware constraints ------------------------------------------- */

static const struct snd_pcm_hardware motu424_pcm_hw = {
	.info = (SNDRV_PCM_INFO_INTERLEAVED |
		SNDRV_PCM_INFO_BLOCK_TRANSFER |
		SNDRV_PCM_INFO_MMAP |
		SNDRV_PCM_INFO_MMAP_VALID |
		SNDRV_PCM_INFO_PAUSE),
	.formats = SNDRV_PCM_FMTBIT_S32_LE,
	.rates = (SNDRV_PCM_RATE_44100 |
		  SNDRV_PCM_RATE_48000 |
		  SNDRV_PCM_RATE_88200 |
		  SNDRV_PCM_RATE_96000),
	.rate_min = 44100,
	.rate_max = 96000,
	.channels_min = 2,
	.channels_max = 2,
	.buffer_bytes_max = MOTU_DMA_BUF_MAX,
	.period_bytes_min = MOTU_DMA_PERIOD_BYTES_MIN,
	.period_bytes_max = MOTU_DMA_PERIOD_BYTES_MAX,
	.periods_min = MOTU_DMA_PERIODS_MIN,
	.periods_max = MOTU_DMA_PERIODS_MAX,
};

/* ---- PCM open/close ----------------------------------------------------- */

static int motu424_playback_open(struct snd_pcm_substream *substream)
{
	struct motu424_linux *motu = snd_pcm_substream_chip(substream);

	motu->playback_substream = substream;
	substream->runtime->hw = motu424_pcm_hw;
	return 0;
}

static int motu424_playback_close(struct snd_pcm_substream *substream)
{
	struct motu424_linux *motu = snd_pcm_substream_chip(substream);
	motu->playback_substream = NULL;
	return 0;
}

static int motu424_capture_open(struct snd_pcm_substream *substream)
{
	struct motu424_linux *motu = snd_pcm_substream_chip(substream);

	motu->capture_substream = substream;
	substream->runtime->hw = motu424_pcm_hw;
	return 0;
}

static int motu424_capture_close(struct snd_pcm_substream *substream)
{
	struct motu424_linux *motu = snd_pcm_substream_chip(substream);
	motu->capture_substream = NULL;
	return 0;
}

/* ---- PCM hw_params / hw_free -------------------------------------------- */

static int motu424_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *hw_params)
{
	/* Managed buffer API handles allocation */
	return 0;
}

static int motu424_hw_free(struct snd_pcm_substream *substream)
{
	return 0;
}

/* ---- PCM prepare -------------------------------------------------------- */

static int motu424_playback_prepare(struct snd_pcm_substream *substream)
{
	struct motu424_linux *motu = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	u32 period_bytes;
	unsigned long flags;

	pal_spinlock_lock_irqsave(motu->ctx.lock, &flags);

	if (motu->ctx.dma_buf)
		memset(motu->ctx.dma_buf, 0, motu->ctx.dma_size);

	period_bytes = frames_to_bytes(runtime, runtime->period_size);
	pal_write32(motu->ctx.iobase_reg, MOTU_REG_PORT_CONF, 0);
	pal_write32(motu->ctx.iobase_reg, MOTU_REG_DMA_SIZE, period_bytes);

	if (motu->ctx.dma_addr)
		pal_write32(motu->ctx.iobase_reg, MOTU_REG_DMA_BASE,
			    (u32)motu->ctx.dma_addr);

	motu->ctx.rate = runtime->rate;
	motu->ctx.dma_running = 0;

	pal_spinlock_unlock_irqrestore(motu->ctx.lock, flags);
	return 0;
}

static int motu424_capture_prepare(struct snd_pcm_substream *substream)
{
	struct motu424_linux *motu = snd_pcm_substream_chip(substream);
	unsigned long flags;

	pal_spinlock_lock_irqsave(motu->ctx.lock, &flags);
	motu->ctx.rate = substream->runtime->rate;
	motu->ctx.dma_running = 0;
	pal_spinlock_unlock_irqrestore(motu->ctx.lock, flags);
	return 0;
}

/* ---- PCM trigger -------------------------------------------------------- */

static int motu424_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct motu424_linux *motu = snd_pcm_substream_chip(substream);
	unsigned long flags;
	u32 val;
	int ret = 0;

	pal_spinlock_lock_irqsave(motu->ctx.lock, &flags);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		val = pal_read32(motu->ctx.iobase_reg, MOTU_REG_PORT_CONF);
		val |= MOTU_PORT_CONF_DMA_EN | MOTU_PORT_CONF_RUN;
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			val |= MOTU_PORT_CONF_DMA_RD;
		else
			val &= ~MOTU_PORT_CONF_DMA_RD;
		pal_write32(motu->ctx.iobase_reg, MOTU_REG_PORT_CONF, val);
		motu->ctx.port_conf_shadow = val;
		motu->ctx.dma_running = 1;
		motu->ctx.start_time_ns = ktime_get_ns();
		break;

	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		val = pal_read32(motu->ctx.iobase_reg, MOTU_REG_PORT_CONF);
		val &= ~(MOTU_PORT_CONF_DMA_EN | MOTU_PORT_CONF_RUN |
			 MOTU_PORT_CONF_DMA_RD | MOTU_PORT_CONF_START);
		pal_write32(motu->ctx.iobase_reg, MOTU_REG_PORT_CONF, val);
		motu->ctx.port_conf_shadow = val;
		motu->ctx.dma_running = 0;
		break;

	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		val = motu->ctx.port_conf_shadow | MOTU_PORT_CONF_DMA_EN;
		pal_write32(motu->ctx.iobase_reg, MOTU_REG_PORT_CONF, val);
		motu->ctx.port_conf_shadow = val;
		motu->ctx.dma_running = 1;
		break;

	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		val = motu->ctx.port_conf_shadow & ~MOTU_PORT_CONF_DMA_EN;
		pal_write32(motu->ctx.iobase_reg, MOTU_REG_PORT_CONF, val);
		motu->ctx.port_conf_shadow = val;
		motu->ctx.dma_running = 0;
		break;

	default:
		ret = -EINVAL;
	}

	pal_spinlock_unlock_irqrestore(motu->ctx.lock, flags);
	return ret;
}

/* ---- PCM pointer -------------------------------------------------------- */

static snd_pcm_uframes_t motu424_pointer(struct snd_pcm_substream *substream)
{
	struct motu424_linux *motu = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	snd_pcm_uframes_t pos = 0;
	u32 hw_ptr;

	hw_ptr = pal_read32(motu->ctx.iobase_reg, MOTU_REG_DMA_CTRL);

	if (motu->ctx.dma_running && hw_ptr > 0 && hw_ptr < motu->ctx.dma_size) {
		pos = bytes_to_frames(runtime, hw_ptr);
		if (pos >= runtime->buffer_size)
			pos = 0;
	} else if (motu->ctx.dma_running && runtime->buffer_size > 0) {
		u64 now = ktime_get_ns();
		u64 elapsed_ns = now - motu->ctx.start_time_ns;
		u64 frames = div_u64(elapsed_ns * motu->ctx.rate, 1000000000ULL);
		pos = frames % runtime->buffer_size;
	}

	return pos;
}

/* ---- PCM ack (stereo <-> multiplexed frame bridging) -------------------- */

static int motu424_playback_ack(struct snd_pcm_substream *substream)
{
	struct motu424_linux *motu = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;

	if (!motu->ctx.dma_buf || !runtime->dma_area)
		return 0;

	{
		u32 *alsa_buf = (u32 *)runtime->dma_area;
		u32 *hw_buf = (u32 *)motu->ctx.dma_buf;
		unsigned long frames = runtime->buffer_size;
		unsigned long i;

		for (i = 0; i < frames; i++) {
			u32 val_l = alsa_buf[i * 2];
			u32 val_r = alsa_buf[i * 2 + 1];
			u32 *frame = hw_buf + (i * MOTU_CHANNELS_PER_FRAME);
			frame[2] = val_l;
			frame[3] = val_r;
		}
	}

	return 0;
}

static int motu424_capture_ack(struct snd_pcm_substream *substream)
{
	struct motu424_linux *motu = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;

	if (!motu->ctx.dma_buf || !runtime->dma_area)
		return 0;

	{
		u32 *alsa_buf = (u32 *)runtime->dma_area;
		const u32 *hw_buf = (const u32 *)motu->ctx.dma_buf;
		unsigned long frames = runtime->buffer_size;
		unsigned long i;

		for (i = 0; i < frames; i++) {
			const u32 *frame = hw_buf + (i * MOTU_CHANNELS_PER_FRAME);
			alsa_buf[i * 2] = frame[2];
			alsa_buf[i * 2 + 1] = frame[3];
		}
	}

	return 0;
}

/* ---- PCM ops structures -------------------------------------------------- */

static const struct snd_pcm_ops motu424_playback_ops = {
	.open		= motu424_playback_open,
	.close		= motu424_playback_close,
	.hw_params	= motu424_hw_params,
	.hw_free	= motu424_hw_free,
	.prepare	= motu424_playback_prepare,
	.trigger	= motu424_trigger,
	.pointer	= motu424_pointer,
	.ack		= motu424_playback_ack,
};

static const struct snd_pcm_ops motu424_capture_ops = {
	.open		= motu424_capture_open,
	.close		= motu424_capture_close,
	.hw_params	= motu424_hw_params,
	.hw_free	= motu424_hw_free,
	.prepare	= motu424_capture_prepare,
	.trigger	= motu424_trigger,
	.pointer	= motu424_pointer,
	.ack		= motu424_capture_ack,
};

/* ---- PCM device creation ------------------------------------------------ */

static int motu424_pcm_new(struct motu424_linux *motu)
{
	struct snd_pcm *pcm;
	int err;

	err = snd_pcm_new(motu->card, "MOTU 424", 0, 1, 1, &pcm);
	if (err < 0)
		return err;

	pcm->private_data = motu;
	strcpy(pcm->name, "MOTU PCI-424");

	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_PLAYBACK,
			&motu424_playback_ops);
	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_CAPTURE,
			&motu424_capture_ops);

	snd_pcm_set_managed_buffer_all(pcm, SNDRV_DMA_TYPE_DEV_SG,
				       &motu->pci->dev,
				       64 * 1024, MOTU_DMA_BUF_MAX);
	return 0;
}

/* ---- Card cleanup ------------------------------------------------------- */

static void motu424_free(struct snd_card *card)
{
	struct motu424_linux *motu = card->private_data;

	if (!motu)
		return;

	/* Stop hardware */
	if (motu->ctx.iobase_reg) {
		pal_write32(motu->ctx.iobase_reg, MOTU_REG_INT_MASK, 0);
		motu424_hw_stop(&motu->ctx);
	}

	/* Free IRQ */
	pal_irq_free(&motu->pal_dev);

	/* Free DMA */
	motu424_dma_free(&motu->ctx);

	/* Free firmware */
	if (motu->fw_fpga)
		release_firmware(motu->fw_fpga);
	if (motu->fw_init)
		release_firmware(motu->fw_init);

	/* Unmap BARs */
	if (motu->ctx.iobase_port)
		pci_iounmap(motu->pci, motu->ctx.iobase_port);
	if (motu->ctx.iobase_reg)
		pci_iounmap(motu->pci, motu->ctx.iobase_reg);
	if (motu->ctx.iobase_dsp)
		pci_iounmap(motu->pci, motu->ctx.iobase_dsp);

	/* Free spinlock */
	if (motu->ctx.lock)
		pal_spinlock_destroy(motu->ctx.lock);

	/* Release PCI */
	if (pci_dev_is_added(motu->pci)) {
		pci_release_regions(motu->pci);
		pci_disable_device(motu->pci);
	}
}

/* ---- PCI probe ---------------------------------------------------------- */

static int motu424_probe(struct pci_dev *pci,
			 const struct pci_device_id *id)
{
	struct snd_card *card;
	struct motu424_linux *motu;
	int err;

	err = snd_card_new(&pci->dev, index, id, THIS_MODULE,
			   sizeof(*motu), &card);
	if (err < 0)
		return err;

	motu = card->private_data;
	memset(motu, 0, sizeof(*motu));
	motu->pci = pci;
	motu->card = card;
	motu->pal_dev.pci = pci;
	motu->pal_dev.irq = -1;
	motu->ctx.dev = &motu->pal_dev;
	motu->ctx.rate = MOTU_RATE_48000;
	motu->ctx.lock = pal_spinlock_create();
	card->private_free = motu424_free;

	strcpy(card->driver, "MOTU_424");
	strcpy(card->shortname, "MOTU PCI-424");
	sprintf(card->longname, "MOTU PCI-424 at %#llx",
		(unsigned long long)pci_resource_start(pci, MOTU_BAR_REG));

	/* 1. Enable PCI */
	err = pal_pci_enable(&motu->pal_dev);
	if (err)
		goto err_card;
	pal_pci_set_master(&motu->pal_dev);
	err = pal_pci_set_dma_mask(&motu->pal_dev, 32);
	if (err) {
		pal_log(PAL_LOG_ERR, "32-bit DMA not supported\n");
		goto err_card;
	}

	/* 2. Map BARs */
	err = pci_request_regions(pci, MOTU_DRIVER_NAME);
	if (err)
		goto err_card;

	motu->ctx.iobase_dsp = pal_iomap(&motu->pal_dev, MOTU_BAR_DSP);
	motu->ctx.iobase_reg = pal_iomap(&motu->pal_dev, MOTU_BAR_REG);
	motu->ctx.iobase_port = pal_iomap(&motu->pal_dev, MOTU_BAR_PORT);
	if (!motu->ctx.iobase_dsp || !motu->ctx.iobase_reg ||
	    !motu->ctx.iobase_port) {
		pal_log(PAL_LOG_ERR, "failed to map BARs\n");
		err = -ENOMEM;
		goto err_card;
	}

	/* 3. Allocate DMA (before init replay for address translation) */
	err = motu424_dma_alloc(&motu->ctx, MOTU_DMA_BUF_MAX);
	if (err) {
		pal_log(PAL_LOG_ERR, "DMA buffer alloc failed\n");
		goto err_card;
	}

	/* 4. Load FPGA firmware (optional — warm boot skips this) */
	err = request_firmware(&motu->fw_fpga, MOTU_FW_FPGA, &pci->dev);
	if (err) {
		pal_log(PAL_LOG_WARN,
			"FPGA firmware not found; assuming warm boot\n");
		motu->fw_fpga = NULL;
	}

	/* 5. Load init sequence firmware */
	err = request_firmware(&motu->fw_init, MOTU_FW_INIT_SEQ, &pci->dev);
	if (err) {
		pal_log(PAL_LOG_WARN,
			"Init sequence not found; skipping (warm boot)\n");
		motu->fw_init = NULL;
	}

	/* 6. Full hardware initialization:
	 *    FPGA load + DSP RAM zero + sequence replay + DSP kick-start +
	 *    port config loop + sync poll */
	{
		const void *fpga_data = motu->fw_fpga ?
			motu->fw_fpga->data : NULL;
		size_t fpga_size = motu->fw_fpga ?
			motu->fw_fpga->size : 0;
		const void *seq_data = motu->fw_init ?
			motu->fw_init->data : NULL;
		size_t seq_size = motu->fw_init ?
			motu->fw_init->size : 0;

		err = motu424_full_init(&motu->ctx, fpga_data, fpga_size,
					seq_data, seq_size);
		if (err)
			pal_log(PAL_LOG_WARN, "Hardware init incomplete\n");
	}

	/* 7. Request IRQ */
	err = pal_irq_request(&motu->pal_dev, motu424_irq_handler, motu);
	if (err) {
		pal_log(PAL_LOG_ERR, "cannot grab IRQ\n");
		goto err_card;
	}
	pal_write32(motu->ctx.iobase_reg, MOTU_REG_INT_MASK,
		    MOTU_INT_PERIOD_ELAPSED);

	/* 8. Create PCM */
	err = motu424_pcm_new(motu);
	if (err) {
		pal_log(PAL_LOG_ERR, "PCM creation failed\n");
		goto err_card;
	}

	/* 9. Register card */
	err = snd_card_register(card);
	if (err)
		goto err_card;

	pci_set_drvdata(pci, card);
	pal_log(PAL_LOG_INFO, "MOTU PCI-424 initialized at %d Hz\n",
		motu->ctx.rate);
	return 0;

err_card:
	snd_card_free(card);
	return err;
}

/* ---- PCI remove --------------------------------------------------------- */

static void motu424_remove(struct pci_dev *pci)
{
	struct snd_card *card = pci_get_drvdata(pci);
	if (card)
		snd_card_free(card);
}

/* ---- PCI ID table ------------------------------------------------------- */

static const struct pci_device_id motu424_ids[] = {
	{ PCI_DEVICE(MOTU_VENDOR_ID, MOTU_DEVICE_ID) },
	{ 0, }
};
MODULE_DEVICE_TABLE(pci, motu424_ids);

/* ---- PCI driver structure ------------------------------------------------ */

static struct pci_driver motu424_driver = {
	.name		= "snd_motu424",
	.id_table	= motu424_ids,
	.probe		= motu424_probe,
	.remove		= motu424_remove,
};

module_pci_driver(motu424_driver);
