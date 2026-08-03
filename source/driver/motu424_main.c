// SPDX-License-Identifier: GPL-2.0
/*
 * motu424_main.c - Main module: PCI probe/remove and ALSA card lifecycle
 *
 * This is the entry point for the snd_motu424 kernel module. It handles
 * PCI device discovery, BAR mapping, firmware loading, hardware
 * initialization, and ALSA card registration.
 *
 * The initialization sequence on probe is:
 *   1. Enable PCI device, set bus master, set DMA mask
 *   2. Request PCI regions and map BAR0/BAR1/BAR2
 *   3. Load FPGA bitstream via BAR1 bitbang (motu424_fpga.c)
 *   4. Replay the golden init sequence to BAR0 (motu424_init.c)
 *   5. Allocate DMA buffers (motu424_dma.c)
 *   6. Request IRQ
 *   7. Create ALSA PCM device (motu424_pcm.c)
 *   8. Register ALSA card
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/pci.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/interrupt.h>
#include <sound/core.h>
#include <sound/initval.h>

#include "motu424.h"

MODULE_AUTHOR("MOTU PCI-424 ALSA Driver Project");
MODULE_DESCRIPTION("ALSA driver for MOTU PCI-424 audio interface");
MODULE_LICENSE("GPL");
MODULE_FIRMWARE(MOTU_FW_FPGA);
MODULE_FIRMWARE(MOTU_FW_INIT_SEQ);

static int index = SNDRV_DEFAULT_IDX1;
static char *id = SNDRV_DEFAULT_STR1;
static bool enable = true;

module_param(index, int, 0444);
module_param(id, charp, 0444);
module_param(enable, bool, 0444);

/* ---- Interrupt handler -------------------------------------------------- */

static irqreturn_t motu424_interrupt(int irq, void *dev_id)
{
	struct motu424 *motu = dev_id;
	u32 status;

	status = ioread32(motu->iobase_reg + MOTU_REG_INT_STATUS);
	if (status == 0 || status == 0xffffffff)
		return IRQ_NONE;

	/* Acknowledge all pending interrupts */
	iowrite32(status, motu->iobase_reg + MOTU_REG_INT_ACK);

	if (status & MOTU_INT_PERIOD_ELAPSED) {
		if (motu->dma_running) {
			if (motu->playback_substream)
				snd_pcm_period_elapsed(motu->playback_substream);
			if (motu->capture_substream)
				snd_pcm_period_elapsed(motu->capture_substream);
		}
	}

	return IRQ_HANDLED;
}

/* ---- Card creation and teardown ---------------------------------------- */

static void motu424_free(struct snd_card *card)
{
	struct motu424 *motu = card->private_data;

	if (!motu)
		return;

	/* Stop hardware: mask interrupts and stop DMA.
	 * Only touch hardware if BARs are still mapped. */
	if (motu->iobase_reg) {
		iowrite32(0, motu->iobase_reg + MOTU_REG_INT_MASK);
		motu424_hw_stop(motu);
	}

	if (motu->irq >= 0)
		free_irq(motu->irq, motu);

	motu424_dma_free(motu);

	if (motu->iobase_port)
		pci_iounmap(motu->pci, motu->iobase_port);
	if (motu->iobase_reg)
		pci_iounmap(motu->pci, motu->iobase_reg);
	if (motu->iobase_dsp)
		pci_iounmap(motu->pci, motu->iobase_dsp);
	motu->iobase_port = motu->iobase_reg = motu->iobase_dsp = NULL;

	if (pci_dev_is_added(motu->pci)) {
		pci_release_regions(motu->pci);
		pci_disable_device(motu->pci);
	}
}

int motu424_create_card(struct pci_dev *pci, struct snd_card **card_out)
{
	struct snd_card *card;
	struct motu424 *motu;
	const struct firmware *fw_fpga = NULL;
	const struct firmware *fw_init = NULL;
	int err;

	err = snd_card_new(&pci->dev, index, id, THIS_MODULE, sizeof(*motu),
			   &card);
	if (err < 0)
		return err;

	/* snd_card_new zeroes private_data, so all fields start at 0/NULL */
	motu = card->private_data;
	motu->pci = pci;
	motu->card = card;
	motu->irq = -1;
	motu->rate = MOTU_RATE_48000;
	spin_lock_init(&motu->lock);
	card->private_free = motu424_free;

	strcpy(card->driver, MOTU_DRIVER_NAME);
	strcpy(card->shortname, "MOTU PCI-424");
	sprintf(card->longname, "MOTU PCI-424 at %#llx, irq %d",
		(unsigned long long)pci_resource_start(pci, MOTU_BAR_REG),
		pci->irq);

	/* 1. Enable PCI device and bus mastering */
	err = pci_enable_device(pci);
	if (err < 0)
		goto err_card;

	pci_set_master(pci);

	err = dma_set_mask_and_coherent(&pci->dev, DMA_BIT_MASK(32));
	if (err) {
		dev_err(&pci->dev, "32-bit DMA not supported\n");
		goto err_card;
	}

	/* 2. Request regions and map BARs */
	err = pci_request_regions(pci, MOTU_DRIVER_NAME);
	if (err < 0)
		goto err_card;

	motu->iobase_dsp = pci_iomap(pci, MOTU_BAR_DSP, 0);
	motu->iobase_reg = pci_iomap(pci, MOTU_BAR_REG, 0);
	motu->iobase_port = pci_iomap(pci, MOTU_BAR_PORT, 0);

	if (!motu->iobase_dsp || !motu->iobase_reg || !motu->iobase_port) {
		dev_err(&pci->dev, "failed to map BARs\n");
		err = -ENOMEM;
		goto err_card;
	}

	dev_info(&pci->dev,
		  "BARs mapped: DSP=%px REG=%px PORT=%px\n",
		  motu->iobase_dsp, motu->iobase_reg, motu->iobase_port);

	/* 3. Allocate DMA buffer — must happen BEFORE init sequence replay
	 *    because the init sequence contains hardcoded physical addresses
	 *    that are translated to point to our DMA buffer. */
	err = motu424_dma_alloc(motu, MOTU_DMA_BUF_MAX);
	if (err < 0) {
		dev_err(&pci->dev, "DMA buffer alloc failed: %d\n", err);
		goto err_card;
	}

	/* 4. Load FPGA bitstream firmware (optional — warm boot skips) */
	err = request_firmware(&fw_fpga, MOTU_FW_FPGA, &pci->dev);
	if (err < 0) {
		dev_warn(&pci->dev,
			 "FPGA firmware %s not found (err=%d); "
			 "assuming warm boot with FPGA already loaded\n",
			 MOTU_FW_FPGA, err);
		fw_fpga = NULL;
	}

	/* 5. Load init sequence firmware (optional — warm boot skips) */
	err = request_firmware(&fw_init, MOTU_FW_INIT_SEQ, &pci->dev);
	if (err < 0) {
		dev_warn(&pci->dev,
			 "Init sequence %s not found (err=%d); "
			 "skipping DSP init (warm boot only)\n",
			 MOTU_FW_INIT_SEQ, err);
		fw_init = NULL;
	}

	/* 6. Full hardware initialization:
	 *    FPGA load + DSP RAM zero + sequence replay + DSP kick-start +
	 *    port config loop + sync poll */
	err = motu424_full_init(motu, fw_fpga, fw_init);
	if (err < 0) {
		dev_err(&pci->dev, "Hardware init failed: %d\n", err);
		if (fw_fpga)
			release_firmware(fw_fpga);
		if (fw_init)
			release_firmware(fw_init);
		goto err_card;
	}
	if (fw_fpga)
		release_firmware(fw_fpga);
	if (fw_init)
		release_firmware(fw_init);

	/* 7. Request IRQ */
	err = request_irq(pci->irq, motu424_interrupt, IRQF_SHARED,
			  KBUILD_MODNAME, motu);
	if (err) {
		dev_err(&pci->dev, "cannot grab IRQ %d\n", pci->irq);
		goto err_card;
	}
	motu->irq = pci->irq;

	/* Unmask interrupts */
	iowrite32(MOTU_INT_PERIOD_ELAPSED, motu->iobase_reg + MOTU_REG_INT_MASK);

	/* 8. Create PCM device */
	err = motu424_pcm_new(motu);
	if (err < 0) {
		dev_err(&pci->dev, "PCM creation failed: %d\n", err);
		goto err_card;
	}

	/* 9. Register ALSA card */
	err = snd_card_register(card);
	if (err < 0) {
		dev_err(&pci->dev, "card registration failed: %d\n", err);
		goto err_card;
	}

	pci_set_drvdata(pci, card);
	*card_out = card;
	dev_info(&pci->dev, "MOTU PCI-424 initialized at %d Hz\n", motu->rate);
	return 0;

err_card:
	/* snd_card_free() triggers motu424_free() which handles all cleanup
	 * with proper null checks for partially-initialized state. */
	snd_card_free(card);
	return err;
}

void motu424_destroy_card(struct snd_card *card)
{
	/* snd_card_free() triggers card->private_free = motu424_free */
	snd_card_free(card);
}

/* ---- PCI driver -------------------------------------------------------- */

static const struct pci_device_id motu424_ids[] = {
	{ PCI_DEVICE(MOTU_VENDOR_ID, MOTU_DEVICE_ID) },
	{ 0, }
};
MODULE_DEVICE_TABLE(pci, motu424_ids);

static int motu424_probe(struct pci_dev *pci,
			 const struct pci_device_id *pci_id)
{
	struct snd_card *card;
	int err;

	if (!enable)
		return -ENOENT;

	dev_info(&pci->dev, "probing MOTU PCI-424 (rev %02x)\n",
		 pci->revision);

	err = motu424_create_card(pci, &card);
	if (err < 0) {
		dev_err(&pci->dev, "probe failed: %d\n", err);
		return err;
	}

	return 0;
}

static void motu424_remove(struct pci_dev *pci)
{
	struct snd_card *card = pci_get_drvdata(pci);

	if (card)
		motu424_destroy_card(card);
}

#ifdef CONFIG_PM_SLEEP
static int motu424_suspend(struct device *dev)
{
	struct snd_card *card = dev_get_drvdata(dev);
	struct motu424 *motu;

	if (!card || !card->private_data)
		return 0;
	motu = card->private_data;

	snd_power_change_state(card, SNDRV_CTL_POWER_D3hot);
	motu424_hw_stop(motu);
	return 0;
}

static int motu424_resume(struct device *dev)
{
	struct snd_card *card = dev_get_drvdata(dev);
	struct motu424 *motu;

	if (!card || !card->private_data)
		return 0;
	motu = card->private_data;

	motu424_hw_init(motu);
	snd_power_change_state(card, SNDRV_CTL_POWER_D0);
	return 0;
}

static SIMPLE_DEV_PM_OPS(motu424_pm, motu424_suspend, motu424_resume);
#define MOTU424_PM_OPS (&motu424_pm)
#else
#define MOTU424_PM_OPS NULL
#endif

static struct pci_driver motu424_driver = {
	.name		= MOTU_DRIVER_NAME,
	.id_table	= motu424_ids,
	.probe		= motu424_probe,
	.remove		= motu424_remove,
	.driver.pm	= MOTU424_PM_OPS,
};

module_pci_driver(motu424_driver);
