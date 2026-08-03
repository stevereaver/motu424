// SPDX-License-Identifier: GPL-2.0
/*
 * motu424_pcm.c - ALSA PCM operations for playback and capture
 *
 * The MOTU PCI-424 uses a multiplexed audio frame format. Each sample
 * period, the card transfers a fixed-size frame containing all channels
 * for all ports:
 *
 *   98 channels * 4 bytes = 392 bytes per frame
 *
 * The first two 32-bit words of each frame are status/magic words that
 * must be preserved. Audio sample data begins at word offset 2.
 *
 * The card DMA-transfers these frames to/from the coherent DMA buffer.
 * The ALSA PCM layer provides a separate (virtually contiguous) buffer
 * for user-space. The driver bridges the two by copying audio data
 * between the ALSA ring buffer and the hardware DMA buffer, expanding
 * stereo ALSA data into the multiplexed frame format on playback and
 * extracting it on capture.
 *
 * The hardware pointer is determined by reading the card's DMA position
 * counter register. If interrupts are not yet reliable, a fallback
 * time-based pointer calculation is used.
 */

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/pci.h>
#include <sound/core.h>
#include <sound/pcm.h>

#include "motu424.h"

/* ---- Hardware constraints ---------------------------------------------- */

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

/* ---- PCM open / close --------------------------------------------------- */

static int motu424_playback_open(struct snd_pcm_substream *substream)
{
	struct motu424 *motu = snd_pcm_substream_chip(substream);
	unsigned long flags;

	spin_lock_irqsave(&motu->lock, flags);
	motu->playback_substream = substream;
	spin_unlock_irqrestore(&motu->lock, flags);

	substream->runtime->hw = motu424_pcm_hw;
	return 0;
}

static int motu424_playback_close(struct snd_pcm_substream *substream)
{
	struct motu424 *motu = snd_pcm_substream_chip(substream);
	unsigned long flags;

	spin_lock_irqsave(&motu->lock, flags);
	motu->playback_substream = NULL;
	spin_unlock_irqrestore(&motu->lock, flags);

	return 0;
}

static int motu424_capture_open(struct snd_pcm_substream *substream)
{
	struct motu424 *motu = snd_pcm_substream_chip(substream);
	unsigned long flags;

	spin_lock_irqsave(&motu->lock, flags);
	motu->capture_substream = substream;
	spin_unlock_irqrestore(&motu->lock, flags);

	substream->runtime->hw = motu424_pcm_hw;
	return 0;
}

static int motu424_capture_close(struct snd_pcm_substream *substream)
{
	struct motu424 *motu = snd_pcm_substream_chip(substream);
	unsigned long flags;

	spin_lock_irqsave(&motu->lock, flags);
	motu->capture_substream = NULL;
	spin_unlock_irqrestore(&motu->lock, flags);

	return 0;
}

/* ---- Hardware parameters ------------------------------------------------ */

static int motu424_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *hw_params)
{
	/* Buffer allocation is handled by the managed buffer API
	 * (snd_pcm_set_managed_buffer_all). Nothing to do here. */
	return 0;
}

static int motu424_hw_free(struct snd_pcm_substream *substream)
{
	/* Nothing to do — managed buffer API handles freeing. */
	return 0;
}

/* ---- Prepare ------------------------------------------------------------ */

static int motu424_playback_prepare(struct snd_pcm_substream *substream)
{
	struct motu424 *motu = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	unsigned long flags;
	u32 period_bytes;

	spin_lock_irqsave(&motu->lock, flags);

	/* Clear the DMA buffer */
	if (motu->dma_buf)
		memset(motu->dma_buf, 0, motu->dma_size);

	/* Program DMA period size into BAR1 registers */
	period_bytes = frames_to_bytes(runtime, runtime->period_size);
	iowrite32(0, motu->iobase_reg + MOTU_REG_DMA_CTRL);
	iowrite32(period_bytes, motu->iobase_reg + MOTU_REG_DMA_SIZE);

	/* Set the DMA base address */
	if (motu->dma_addr)
		iowrite32((u32)motu->dma_addr, motu->iobase_reg + MOTU_REG_DMA_BASE);

	motu->rate = runtime->rate;
	motu->dma_running = 0;

	spin_unlock_irqrestore(&motu->lock, flags);

	return 0;
}

static int motu424_capture_prepare(struct snd_pcm_substream *substream)
{
	struct motu424 *motu = snd_pcm_substream_chip(substream);
	unsigned long flags;

	spin_lock_irqsave(&motu->lock, flags);
	motu->rate = substream->runtime->rate;
	motu->dma_running = 0;
	spin_unlock_irqrestore(&motu->lock, flags);

	return 0;
}

/* ---- Trigger (start/stop DMA) ------------------------------------------- */

static int motu424_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct motu424 *motu = snd_pcm_substream_chip(substream);
	unsigned long flags;
	u32 val;
	int ret = 0;

	spin_lock_irqsave(&motu->lock, flags);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		/* Enable DMA + run bits in port config.
		 * DMA_RD means the card reads from memory (playback).
		 * For capture, clear DMA_RD so the card writes to memory. */
		val = ioread32(motu->iobase_reg + MOTU_REG_PORT_CONF);
		val |= MOTU_PORT_CONF_DMA_EN | MOTU_PORT_CONF_RUN;
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			val |= MOTU_PORT_CONF_DMA_RD;
		else
			val &= ~MOTU_PORT_CONF_DMA_RD;
		iowrite32(val, motu->iobase_reg + MOTU_REG_PORT_CONF);
		motu->port_conf_shadow = val;
		motu->dma_running = 1;
		motu->start_time_ns = ktime_get_ns();
		break;

	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		val = ioread32(motu->iobase_reg + MOTU_REG_PORT_CONF);
		val &= ~(MOTU_PORT_CONF_DMA_EN | MOTU_PORT_CONF_RUN |
			 MOTU_PORT_CONF_DMA_RD | MOTU_PORT_CONF_START);
		iowrite32(val, motu->iobase_reg + MOTU_REG_PORT_CONF);
		motu->port_conf_shadow = val;
		motu->dma_running = 0;
		break;

	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		val = motu->port_conf_shadow | MOTU_PORT_CONF_DMA_EN;
		iowrite32(val, motu->iobase_reg + MOTU_REG_PORT_CONF);
		motu->port_conf_shadow = val;
		motu->dma_running = 1;
		break;

	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		val = motu->port_conf_shadow & ~MOTU_PORT_CONF_DMA_EN;
		iowrite32(val, motu->iobase_reg + MOTU_REG_PORT_CONF);
		motu->port_conf_shadow = val;
		motu->dma_running = 0;
		break;

	default:
		ret = -EINVAL;
	}

	spin_unlock_irqrestore(&motu->lock, flags);
	return ret;
}

/* ---- Pointer (DMA position) -------------------------------------------- */

static snd_pcm_uframes_t motu424_pointer(struct snd_pcm_substream *substream)
{
	struct motu424 *motu = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	snd_pcm_uframes_t pos = 0;
	u32 hw_ptr;

	/* Try reading the hardware DMA position counter from BAR1.
	 * If it returns a sane value, use it; otherwise fall back to
	 * a time-based estimate. */
	hw_ptr = ioread32(motu->iobase_reg + MOTU_REG_DMA_CTRL);

	if (motu->dma_running && hw_ptr > 0 && hw_ptr < motu->dma_size) {
		/* Convert byte position to frames */
		pos = bytes_to_frames(runtime, hw_ptr);
		if (pos >= runtime->buffer_size)
			pos = 0;
	} else if (motu->dma_running && runtime->buffer_size > 0) {
		/* Fallback: time-based pointer estimation */
		u64 now = ktime_get_ns();
		u64 elapsed_ns = now - motu->start_time_ns;
		u64 frames = div_u64(elapsed_ns * motu->rate, 1000000000ULL);
		pos = frames % runtime->buffer_size;
	}

	return pos;
}

/* ---- Ack (copy ALSA buffer to hardware DMA buffer) ---------------------- */
/*
 * The MOTU card uses a multiplexed frame format (98 channels * 4 bytes).
 * ALSA provides stereo interleaved data. We expand the stereo data into
 * the multiplexed frame, placing left/right at word offsets 2 and 3.
 * Status words (0 and 1) are preserved.
 */

static int motu424_playback_ack(struct snd_pcm_substream *substream)
{
	struct motu424 *motu = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;

	if (!motu->dma_buf || !runtime->dma_area)
		return 0;

	{
		u32 *alsa_buf = (u32 *)runtime->dma_area;
		u32 *hw_buf = (u32 *)motu->dma_buf;
		unsigned long frames = runtime->buffer_size;
		unsigned long i;

		for (i = 0; i < frames; i++) {
			u32 val_l = alsa_buf[i * 2];
			u32 val_r = alsa_buf[i * 2 + 1];
			u32 *frame = hw_buf + (i * MOTU_CHANNELS_PER_FRAME);

			/* Write audio data at word offset 2 (after status words) */
			frame[2] = val_l;
			frame[3] = val_r;
		}
	}

	return 0;
}

/* ---- Capture: extract audio from hardware DMA buffer ------------------- */

static int motu424_capture_ack(struct snd_pcm_substream *substream)
{
	struct motu424 *motu = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;

	if (!motu->dma_buf || !runtime->dma_area)
		return 0;

	{
		u32 *alsa_buf = (u32 *)runtime->dma_area;
		const u32 *hw_buf = (const u32 *)motu->dma_buf;
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

/* ---- PCM operations structures ------------------------------------------ */

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

int motu424_pcm_new(struct motu424 *motu)
{
	struct snd_pcm *pcm;
	int err;

	/* Create PCM with 1 playback and 1 capture substream */
	err = snd_pcm_new(motu->card, "MOTU 424", 0, 1, 1, &pcm);
	if (err < 0)
		return err;

	pcm->private_data = motu;
	strcpy(pcm->name, "MOTU PCI-424");

	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_PLAYBACK, &motu424_playback_ops);
	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_CAPTURE, &motu424_capture_ops);

	/* Preallocate DMA buffers for the ALSA PCM layer.
	 * This is separate from the hardware DMA buffer — it's the
	 * user-accessible ring buffer. */
	snd_pcm_set_managed_buffer_all(pcm, SNDRV_DMA_TYPE_DEV_SG,
				       &motu->pci->dev,
				       64 * 1024, MOTU_DMA_BUF_MAX);

	return 0;
}
