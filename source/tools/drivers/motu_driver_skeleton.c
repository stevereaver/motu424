// SPDX-License-Identifier: GPL-2.0

#include <linux/module.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <sound/core.h>
#include <sound/initval.h>
#include <sound/pcm.h>

#define MOTU_VENDOR_ID 0x137a // Mark of the Unicorn Inc
#define MOTU_DEVICE_ID 0x0004 // Provided in prompt
#define DRIVER_NAME "snd_motu_pci"

MODULE_AUTHOR("Senior Linux Kernel Developer");
MODULE_DESCRIPTION("ALSA driver for MOTU PCI Audio Interface");
MODULE_LICENSE("GPL");

struct motu_device {
    struct pci_dev *pci;
    struct snd_card *card;
    struct snd_pcm *pcm;
    
    void __iomem *mmio_base;
    int irq;

    // Add spinlocks, DMA buffer pointers, state tracking here
    spinlock_t lock;
};

/* 
 * -------------------------------------------------------------------------
 * ALSA PCM Operations (Advanced Implementation Snippets)
 * -------------------------------------------------------------------------
 */

static int snd_motu_pcm_hw_params(struct snd_pcm_substream *substream,
                                  struct snd_pcm_hw_params *hw_params)
{
    // struct motu_device *motu = snd_pcm_substream_chip(substream);
    
    // ALSA handles the allocation based on the preallocation setup.
    // Here we would typically program the hardware's scatter-gather table
    // or DMA base address registers based on the allocated memory.
    
    // Example: For simple contiguous DMA:
    // dma_addr_t dma_addr = substream->runtime->dma_addr;
    // iowrite32(dma_addr, motu->mmio_base + MOTU_REG_DMA_BASE);
    
    return 0; // snd_pcm_lib_malloc_pages(substream, params_buffer_bytes(hw_params)); is deprecated, handled implicitly usually or explicitly if needed.
}

static int snd_motu_pcm_hw_free(struct snd_pcm_substream *substream)
{
    return 0; // snd_pcm_lib_free_pages(substream);
}

static int snd_motu_pcm_prepare(struct snd_pcm_substream *substream)
{
    struct motu_device *motu = snd_pcm_substream_chip(substream);
    struct snd_pcm_runtime *runtime = substream->runtime;

    spin_lock_irq(&motu->lock);
    
    // Reset DMA engine
    // Program sample rate, channels, format into hardware registers
    // e.g., iowrite32(rate_code, motu->mmio_base + MOTU_REG_SAMPLE_RATE);
    
    spin_unlock_irq(&motu->lock);
    return 0;
}

static int snd_motu_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
    struct motu_device *motu = snd_pcm_substream_chip(substream);
    int ret = 0;

    spin_lock(&motu->lock);

    switch (cmd) {
    case SNDRV_PCM_TRIGGER_START:
    case SNDRV_PCM_TRIGGER_RESUME:
        // Start hardware DMA engine
        // e.g., iowrite32(START_BIT, motu->mmio_base + MOTU_REG_DMA_CTRL);
        break;
    case SNDRV_PCM_TRIGGER_STOP:
    case SNDRV_PCM_TRIGGER_SUSPEND:
        // Stop hardware DMA engine
        // e.g., iowrite32(STOP_BIT, motu->mmio_base + MOTU_REG_DMA_CTRL);
        break;
    default:
        ret = -EINVAL;
    }

    spin_unlock(&motu->lock);
    return ret;
}

static snd_pcm_uframes_t snd_motu_pcm_pointer(struct snd_pcm_substream *substream)
{
    struct motu_device *motu = snd_pcm_substream_chip(substream);
    struct snd_pcm_runtime *runtime = substream->runtime;
    unsigned int hw_ptr = 0;

    // Read current DMA position from hardware register
    // hw_ptr = ioread32(motu->mmio_base + MOTU_REG_DMA_POINTER);
    
    // Convert hardware position (usually bytes) to frames
    // hw_ptr = bytes_to_frames(runtime, hw_ptr);

    // Ensure it wraps correctly according to buffer size
    // if (hw_ptr >= runtime->buffer_size) hw_ptr = 0;

    return hw_ptr;
}

static const struct snd_pcm_ops snd_motu_playback_ops = {
    .open       = NULL, // TODO: Implement open (set runtime->hw)
    .close      = NULL, // TODO: Implement close
    .ioctl      = snd_pcm_lib_ioctl,
    .hw_params  = snd_motu_pcm_hw_params,
    .hw_free    = snd_motu_pcm_hw_free,
    .prepare    = snd_motu_pcm_prepare,
    .trigger    = snd_motu_pcm_trigger,
    .pointer    = snd_motu_pcm_pointer,
    // .copy_user or .mmap ops might be needed for non-standard DMA
};

/* 
 * -------------------------------------------------------------------------
 * Interrupt Handler
 * -------------------------------------------------------------------------
 */
static irqreturn_t snd_motu_interrupt(int irq, void *dev_id)
{
    struct motu_device *motu = dev_id;
    unsigned int status;

    // 1. Read interrupt status register to check if it's our device
    // status = ioread32(motu->mmio_base + MOTU_REG_INT_STATUS);
    status = 0; // Dummy

    if (!status) {
        return IRQ_NONE; // Not our interrupt
    }

    // 2. Acknowledge interrupt
    // iowrite32(status, motu->mmio_base + MOTU_REG_INT_ACK);

    // 3. Handle specific events (e.g., period elapsed for ALSA)
    // if (status & MOTU_INT_PERIOD_ELAPSED) {
    //     if (motu->pcm && motu->pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream) {
    //         snd_pcm_period_elapsed(motu->pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream);
    //     }
    // }

    return IRQ_HANDLED;
}

/* 
 * -------------------------------------------------------------------------
 * Initialization and Probing
 * -------------------------------------------------------------------------
 */
static int snd_motu_probe(struct pci_dev *pci, const struct pci_device_id *pci_id)
{
    struct snd_card *card;
    struct motu_device *motu;
    int err;

    dev_info(&pci->dev, "Probing MOTU PCI audio device...\n");

    // 1. Enable PCI Device
    err = pci_enable_device(pci);
    if (err < 0) return err;

    // Enable bus mastering for DMA
    pci_set_master(pci);

    // 2. Create ALSA Card Instance
    err = snd_card_new(&pci->dev, -1, "MOTU_PCI", THIS_MODULE, sizeof(struct motu_device), &card);
    if (err < 0) {
        pci_disable_device(pci);
        return err;
    }

    motu = card->private_data;
    motu->pci = pci;
    motu->card = card;
    spin_lock_init(&motu->lock);

    strcpy(card->driver, DRIVER_NAME);
    strcpy(card->shortname, "MOTU PCI");
    sprintf(card->longname, "%s at %#llx, irq %i",
            card->shortname,
            (unsigned long long)pci_resource_start(pci, 0),
            pci->irq);

    // 3. Request PCI Regions and Map MMIO
    err = pci_request_regions(pci, DRIVER_NAME);
    if (err < 0) goto error;

    // Assuming BAR 0 is MMIO. Check lspci output to confirm!
    motu->mmio_base = pci_ioremap_bar(pci, 0); 
    if (!motu->mmio_base) {
        err = -EBUSY;
        goto error_regions;
    }

    // 4. Request IRQ
    err = request_irq(pci->irq, snd_motu_interrupt, IRQF_SHARED, KBUILD_MODNAME, motu);
    if (err) {
        dev_err(&pci->dev, "unable to grab IRQ %d\n", pci->irq);
        goto error_iomap;
    }
    motu->irq = pci->irq;

    // 5. Create PCM Device
    err = snd_pcm_new(card, "MOTU Analog", 0, 1, 1, &motu->pcm); // 1 playback, 1 capture substream
    if (err < 0) goto error_irq;

    motu->pcm->private_data = motu;
    strcpy(motu->pcm->name, "MOTU PCI PCM");

    // Register PCM operations
    snd_pcm_set_ops(motu->pcm, SNDRV_PCM_STREAM_PLAYBACK, &snd_motu_playback_ops);
    // snd_pcm_set_ops(motu->pcm, SNDRV_PCM_STREAM_CAPTURE, &snd_motu_capture_ops);

    // Preallocate DMA buffers (Assuming Scatter-Gather for high bandwidth)
    // NOTE: Requires implementing scatter-gather building in hw_params if used.
    snd_pcm_lib_preallocate_pages_for_all(motu->pcm, SNDRV_DMA_TYPE_DEV_SG,
                                          &pci->dev, 64*1024, 1024*1024);

    // 6. Register ALSA Card
    err = snd_card_register(card);
    if (err < 0) goto error_irq;

    pci_set_drvdata(pci, card);
    dev_info(&pci->dev, "MOTU PCI initialized successfully.\n");
    return 0;

error_irq:
    free_irq(motu->irq, motu);
error_iomap:
    pci_iounmap(pci, motu->mmio_base);
error_regions:
    pci_release_regions(pci);
error:
    snd_card_free(card);
    pci_disable_device(pci);
    return err;
}

static void snd_motu_remove(struct pci_dev *pci)
{
    struct snd_card *card = pci_get_drvdata(pci);
    struct motu_device *motu = card->private_data;

    // Stop hardware, mask interrupts
    // e.g., iowrite32(0, motu->mmio_base + MOTU_REG_INT_MASK);

    if (motu->irq >= 0)
        free_irq(motu->irq, motu);
    if (motu->mmio_base)
        pci_iounmap(pci, motu->mmio_base);
    
    pci_release_regions(pci);
    snd_card_free(card);
    pci_disable_device(pci);
}

static const struct pci_device_id snd_motu_ids[] = {
    { PCI_DEVICE(MOTU_VENDOR_ID, MOTU_DEVICE_ID) },
    { 0, }
};
MODULE_DEVICE_TABLE(pci, snd_motu_ids);

static struct pci_driver snd_motu_driver = {
    .name = KBUILD_MODNAME,
    .id_table = snd_motu_ids,
    .probe = snd_motu_probe,
    .remove = snd_motu_remove,
};

module_pci_driver(snd_motu_driver);
