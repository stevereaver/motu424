// SPDX-License-Identifier: GPL-2.0

#include <linux/module.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/vmalloc.h>
#include <linux/firmware.h>
#include <sound/core.h>
#include <sound/initval.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>

#define DRIVER_NAME "snd_motu_pci"
#define MOTU_VENDOR_ID 0x137a 
#define MOTU_DEVICE_ID 0x0004

#define MOTU_REG_PORT_SIZE    0x80
#define MOTU_REG_CLOCK_COUNT  0x1c
#define MOTU_REG_PORT_CONF    0x00
#define MOTU_REG_DMA_BASE     0x18
#define MOTU_REG_INT_STATUS   0x20
#define MOTU_REG_INT_ACK      0x20

struct motu_pci {
    struct pci_dev *pci;
    struct snd_card *card;
    struct snd_pcm *pcm;
    struct snd_pcm_substream *playback_substream;

    void __iomem *iobase_dsp; 
    void __iomem *iobase_reg; 
    void __iomem *iobase_port;
    int irq;

    spinlock_t lock;
    unsigned int dma_running;
    u64 start_time_ns;
    struct timer_list period_timer;

    __le32 *sg_table;
    dma_addr_t sg_table_dma;
    
    void *shadow_buf;
    struct page **shadow_pages;
    int shadow_num_pages;
    dma_addr_t shadow_buf_dma;
    size_t shadow_buf_size;
};

static void motu_timer_function(struct timer_list *t)
{
    struct motu_pci *motu = from_timer(motu, t, period_timer);
    
    if (motu->dma_running && motu->playback_substream) {
        static int tick = 0;
        if ((tick++ % 10) == 0) {
            u32 r00 = ioread32(motu->iobase_reg + 0x00);
            u32 r18 = ioread32(motu->iobase_reg + 0x18);
            u32 r1c = ioread32(motu->iobase_reg + 0x1c);
            u32 r20 = ioread32(motu->iobase_reg + 0x20);
            dev_info(&motu->pci->dev, "Timer Tick: r00=%08x, r18=%08x, r1c=%08x, r20=%08x\n", r00, r18, r1c, r20);
        }
        snd_pcm_period_elapsed(motu->playback_substream);
        /* Re-arm timer for ~10ms */
        mod_timer(&motu->period_timer, jiffies + msecs_to_jiffies(10));
    }
}

static void motu_dump_registers(struct motu_pci *motu)
{
    int i;
    dev_info(&motu->pci->dev, "--- MOTU Port 0 Dump ---\n");
    for (i = 0; i < 0x80; i += 4) {
        u32 val = ioread32(motu->iobase_reg + i);
        if (val != 0 && val != 0xffffffff)
            dev_info(&motu->pci->dev, "Offset 0x%02x: 0x%08x\n", i, val);
    }
}

MODULE_AUTHOR("Senior Linux Kernel Developer");
MODULE_DESCRIPTION("ALSA driver for MOTU PCI Audio Interface");
MODULE_LICENSE("GPL");

static const struct snd_pcm_hardware snd_motu_playback_hw = {
    .info = (SNDRV_PCM_INFO_INTERLEAVED |
             SNDRV_PCM_INFO_BLOCK_TRANSFER |
             SNDRV_PCM_INFO_MMAP |
             SNDRV_PCM_INFO_MMAP_VALID |
             SNDRV_PCM_INFO_PAUSE |
             SNDRV_PCM_INFO_SYNC_START),
    .formats = SNDRV_PCM_FMTBIT_S32_LE,
    .rates = SNDRV_PCM_RATE_48000,
    .rate_min = 48000,
    .rate_max = 48000,
    .channels_min = 2,
    .channels_max = 2,
    .buffer_bytes_max = 32 * 1024,
    .period_bytes_min = 1024,
    .period_bytes_max = 16 * 1024,
    .periods_min = 2,
    .periods_max = 16,
};
static int snd_motu_open(struct snd_pcm_substream *substream)
{
    struct motu_pci *motu = snd_pcm_substream_chip(substream);
    substream->runtime->hw = snd_motu_playback_hw;
    motu->playback_substream = substream;
    return 0;
}

static int snd_motu_close(struct snd_pcm_substream *substream)
{
    struct motu_pci *motu = snd_pcm_substream_chip(substream);
    motu->playback_substream = NULL;
    return 0;
}

static int snd_motu_hw_params(struct snd_pcm_substream *substream,
                              struct snd_pcm_hw_params *hw_params)
{
    struct motu_pci *motu = snd_pcm_substream_chip(substream);
    int err;
    unsigned int hw_bytes = params_buffer_size(hw_params) * 392;

    err = snd_pcm_lib_malloc_pages(substream, params_buffer_bytes(hw_params));
    if (err < 0) return err;

    if (motu->shadow_buf && motu->shadow_buf_size < hw_bytes) {
        dma_free_coherent(&motu->pci->dev, motu->shadow_buf_size, motu->shadow_buf, motu->shadow_buf_dma);
        motu->shadow_buf = NULL;
    }

    if (!motu->shadow_buf) {
        motu->shadow_buf_size = hw_bytes;
        // Limit to 4MB
        motu->shadow_buf = dma_alloc_coherent(&motu->pci->dev, motu->shadow_buf_size, &motu->shadow_buf_dma, GFP_KERNEL);
        if (!motu->shadow_buf) return -ENOMEM;
        memset(motu->shadow_buf, 0, motu->shadow_buf_size);
    }
    
    return 0;
}

static int snd_motu_hw_free(struct snd_pcm_substream *substream)
{
    struct motu_pci *motu = snd_pcm_substream_chip(substream);
    if (motu->shadow_buf) {
        dma_free_coherent(&motu->pci->dev, motu->shadow_buf_size, motu->shadow_buf, motu->shadow_buf_dma);
        motu->shadow_buf = NULL;
    }
    return snd_pcm_lib_free_pages(substream);
}

static int snd_motu_prepare(struct snd_pcm_substream *substream)
{
    struct motu_pci *motu = snd_pcm_substream_chip(substream);
    struct snd_pcm_runtime *runtime = substream->runtime;
    u32 dma_size = runtime->period_size * 392; 

    if (motu->shadow_buf) {
        u32 *frame = (u32 *)motu->shadow_buf;
        int frame_idx;
        for (frame_idx = 0; frame_idx < runtime->buffer_size; frame_idx++) {
            u32 *ptr = frame + (frame_idx * 98);
            
            // Fuzzing with Big Endian magic words
            if (frame_idx < 32) {
                ptr[0] = cpu_to_be32(1 << frame_idx);
                ptr[1] = 0;
            } else if (frame_idx < 64) {
                ptr[0] = 0;
                ptr[1] = cpu_to_be32(1 << (frame_idx - 32));
            } else {
                ptr[0] = 0x00000000;
                ptr[1] = 0x00000000;
            }
            
            int ch;
            for (ch = 2; ch < 98; ch++) {
                ptr[ch] = (frame_idx & 0x20) ? 0x7FFFFFFF : 0x80000000;
            }
        }
    }

    iowrite32(0, motu->iobase_reg + 0x10); 
    iowrite32(dma_size, motu->iobase_reg + 0x14); 

    if (motu->sg_table) {
        iowrite32((u32)motu->sg_table_dma, motu->iobase_reg + MOTU_REG_DMA_BASE); 
    } else {
        iowrite32(0, motu->iobase_reg + MOTU_REG_DMA_BASE); 
    }

    return 0;
}
static int snd_motu_trigger(struct snd_pcm_substream *substream, int cmd)
{
    struct motu_pci *motu = snd_pcm_substream_chip(substream);
    unsigned long flags;
    u32 val;

    spin_lock_irqsave(&motu->lock, flags);
    switch (cmd) {
    case SNDRV_PCM_TRIGGER_START:
        val = ioread32(motu->iobase_reg + MOTU_REG_PORT_CONF);
        iowrite32(val | 0x80 | 0x20000 | 0x40000, motu->iobase_reg + MOTU_REG_PORT_CONF);
        motu->start_time_ns = ktime_get_ns();
        motu->dma_running = 1;
        mod_timer(&motu->period_timer, jiffies + msecs_to_jiffies(10));
        break;
    case SNDRV_PCM_TRIGGER_STOP:
        val = ioread32(motu->iobase_reg + MOTU_REG_PORT_CONF);
        iowrite32(val & ~(0x2 | 0x80 | 0x4000 | 0x8000 | 0x80000000), motu->iobase_reg + MOTU_REG_PORT_CONF);
        motu->dma_running = 0;
        timer_delete_sync(&motu->period_timer);
        break;
    default:
        spin_unlock_irqrestore(&motu->lock, flags);
        return -EINVAL;
    }
    spin_unlock_irqrestore(&motu->lock, flags);
    return 0;
}

static snd_pcm_uframes_t snd_motu_pointer(struct snd_pcm_substream *substream)
{
    struct motu_pci *motu = snd_pcm_substream_chip(substream);
    struct snd_pcm_runtime *runtime = substream->runtime;
    snd_pcm_uframes_t pos = 0;

    if (motu->dma_running && runtime && runtime->buffer_size > 0) {
        u64 now = ktime_get_ns();
        u64 elapsed_ns = now - motu->start_time_ns;
        u64 frames = div_u64(elapsed_ns * 48, 1000000);
        pos = frames % runtime->buffer_size;
    }
    return pos;
}

static int snd_motu_ack(struct snd_pcm_substream *substream)
{
    struct motu_pci *motu = snd_pcm_substream_chip(substream);
    struct snd_pcm_runtime *runtime = substream->runtime;

    // Check if buffers are available
    if (!motu->shadow_buf || !runtime->dma_area) return 0;

    unsigned long buffer_size = runtime->buffer_size;
    unsigned long frames;
    unsigned long i;

    // To make it robust against wrap-arounds for prototyping,
    // let's just do a naive copy of the entire ALSA ring buffer to the shadow buffer
    // on every ack. ALSA ring buffer is relatively small (e.g. 16KB).
    // The hardware reads from shadow_buf.

    u32 *alsa_buf = (u32 *)runtime->dma_area;
    u32 *hwbuf = (u32 *)motu->shadow_buf; 

    frames = buffer_size;
    if (frames == 0) return 0;

    for (i = 0; i < frames; i++) {
        // ALSA stereo buffer: 2 channels * 4 bytes = 8 bytes per frame
        u32 val_l = alsa_buf[i * 2 + 0];
        u32 val_r = alsa_buf[i * 2 + 1];

        // Hardware multiplexed buffer: 98 channels * 4 bytes = 392 bytes per frame (98 words)
        u32 *frame_ptr = hwbuf + (i * 98);

        // Do NOT overwrite Status 0 and Status 1
        frame_ptr[2] = val_l;      // Port 0, Ch 1 (Left)
        frame_ptr[3] = val_r;      // Port 0, Ch 2 (Right)
    }

    return 0;
}

static const struct snd_pcm_ops snd_motu_playback_ops = {
    .open = snd_motu_open,
    .close = snd_motu_close,
    .ioctl = snd_pcm_lib_ioctl,
    .hw_params = snd_motu_hw_params,
    .hw_free = snd_motu_hw_free,
    .prepare = snd_motu_prepare,
    .trigger = snd_motu_trigger,
    .pointer = snd_motu_pointer,
    .ack = snd_motu_ack,
};

static int snd_motu_pcm_new(struct motu_pci *motu)
{
    struct snd_pcm *pcm;
    int err = snd_pcm_new(motu->card, "MOTU PCM", 0, 1, 0, &pcm);
    if (err < 0) return err;
    pcm->private_data = motu;
    snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_PLAYBACK, &snd_motu_playback_ops);
    snd_pcm_set_managed_buffer_all(pcm, SNDRV_DMA_TYPE_DEV_SG, &motu->pci->dev, 64*1024, 1024*1024);
    return 0;
}

static irqreturn_t snd_motu_interrupt(int irq, void *dev_id)
{
    struct motu_pci *motu = dev_id;
    u32 status;

    status = ioread32(motu->iobase_reg + MOTU_REG_INT_STATUS);
    if (status == 0 || status == 0xffffffff)
        return IRQ_NONE;

    dev_info(&motu->pci->dev, "INTERRUPT FIRED! Status: 0x%08x\n", status);
    iowrite32(status, motu->iobase_reg + MOTU_REG_INT_ACK);

    if (motu->playback_substream && motu->dma_running) {
        snd_pcm_period_elapsed(motu->playback_substream);
    }

    return IRQ_HANDLED;
}

MODULE_FIRMWARE("motu/altera424b.rbf");

static int motu_load_fpga(struct motu_pci *motu, const struct firmware *fw)
{
    u32 __iomem *port = motu->iobase_port;
    u32 val;
    
    dev_info(&motu->pci->dev, "Starting FPGA firmware upload (size: %zu)...\n", fw->size);

    if (!port) {
        dev_err(&motu->pci->dev, "No I/O port available for firmware upload.\n");
        return -EIO;
    }

    // Toggle reset
    val = ioread32(motu->iobase_reg + MOTU_REG_PORT_CONF);
    iowrite32(val & ~0x10000, motu->iobase_reg + MOTU_REG_PORT_CONF);
    msleep(50);
    iowrite32(val | 0x10000, motu->iobase_reg + MOTU_REG_PORT_CONF);
    msleep(50);

    dev_info(&motu->pci->dev, "Pumping %zu bytes to I/O Port 0x00...\n", fw->size);
    iowrite32_rep(port, fw->data, fw->size / 4);
    
    msleep(100);
    
    dev_info(&motu->pci->dev, "FPGA firmware upload complete.\n");
    return 0;
}

static int snd_motu_probe(struct pci_dev *pci, const struct pci_device_id *pci_id)
{
    struct snd_card *card;
    struct motu_pci *motu;
    int err;

    const struct firmware *fw;

    err = snd_card_new(&pci->dev, SNDRV_DEFAULT_IDX1, SNDRV_DEFAULT_STR1, THIS_MODULE, sizeof(*motu), &card);
    if (err < 0) return err;

    motu = card->private_data;
    motu->card = card;
    motu->pci = pci;
    spin_lock_init(&motu->lock);
    timer_setup(&motu->period_timer, motu_timer_function, 0);

    if (pci_enable_device(pci) < 0) { snd_card_free(card); return -EIO; }
    pci_set_master(pci);
    
    if (dma_set_mask_and_coherent(&pci->dev, DMA_BIT_MASK(32))) {
        dev_err(&pci->dev, "Architecture does not support 32-bit PCI busmaster DMA\n");
        pci_disable_device(pci);
        snd_card_free(card);
        return -EIO;
    }

    if (pci_request_regions(pci, DRIVER_NAME) < 0) { pci_disable_device(pci); snd_card_free(card); return -EIO; }

    motu->iobase_dsp = pci_iomap(pci, 0, 0);
    motu->iobase_reg = pci_iomap(pci, 1, 0);
    motu->iobase_port = pci_iomap(pci, 2, 0);

    /* Dump I/O Ports */
    if (motu->iobase_port) {
        dev_info(&pci->dev, "--- BAR2 I/O Port Dump ---\n");
        dev_info(&pci->dev, "Port 0x00: %08x\n", ioread32(motu->iobase_port + 0x00));
        dev_info(&pci->dev, "Port 0x04: %08x\n", ioread32(motu->iobase_port + 0x04));
        dev_info(&pci->dev, "Port 0x08: %08x\n", ioread32(motu->iobase_port + 0x08));
        dev_info(&pci->dev, "Port 0x0C: %08x\n", ioread32(motu->iobase_port + 0x0C));
    }

    /* Request and load FPGA firmware */
    err = request_firmware(&fw, "motu/altera424b.rbf", &pci->dev);
    if (err < 0) {
        dev_err(&pci->dev, "Failed to load firmware motu/altera424b.rbf (err=%d)\n", err);
        /* Try to proceed anyway if it was already loaded from a warm boot? Or fail? */
    } else {
        motu_load_fpga(motu, fw);
        release_firmware(fw);
    }

    /* Test if 0x04, 0x08, 0x10, 0x14 are writable */
    dev_info(&pci->dev, "Testing writable registers...\n");
    iowrite32(0x11111111, motu->iobase_reg + 0x04);
    iowrite32(0x22222222, motu->iobase_reg + 0x08);
    iowrite32(0x44444444, motu->iobase_reg + 0x10);
    iowrite32(0x55555555, motu->iobase_reg + 0x14);
    
    dev_info(&pci->dev, "Readback: 0x04: %08x, 0x08: %08x, 0x10: %08x, 0x14: %08x\n",
        ioread32(motu->iobase_reg + 0x04),
        ioread32(motu->iobase_reg + 0x08),
        ioread32(motu->iobase_reg + 0x10),
        ioread32(motu->iobase_reg + 0x14));

    err = request_irq(pci->irq, snd_motu_interrupt, IRQF_SHARED, KBUILD_MODNAME, motu);
    if (err) {
        dev_err(&pci->dev, "unable to grab IRQ %d\n", pci->irq);
        pci_iounmap(pci, motu->iobase_reg); pci_iounmap(pci, motu->iobase_dsp);
        pci_release_regions(pci); pci_disable_device(pci); snd_card_free(card);
        return err;
    }
    motu->irq = pci->irq;

    // Mask interrupts on the hardware just in case it is screaming
    iowrite32(0, motu->iobase_reg + 0x24); /* Often INT_MASK is near INT_STATUS */

    if (snd_motu_pcm_new(motu) < 0) {
        free_irq(motu->irq, motu);
        pci_iounmap(pci, motu->iobase_reg); pci_iounmap(pci, motu->iobase_dsp);
        pci_release_regions(pci); pci_disable_device(pci); snd_card_free(card);
        return -EIO;
    }

    strcpy(card->driver, "MOTU");
    strcpy(card->shortname, "MOTU PCI");
    err = snd_card_register(card);
    if (err < 0) { 
        free_irq(motu->irq, motu);
        pci_iounmap(pci, motu->iobase_reg); pci_iounmap(pci, motu->iobase_dsp);
        pci_release_regions(pci); pci_disable_device(pci); snd_card_free(card);
        return err; 
    }

    pci_set_drvdata(pci, card);
    dev_info(&pci->dev, "MOTU PCI Driver Loaded - HW Counter Version\n");
    motu_dump_registers(motu);
    return 0;
}

static void snd_motu_remove(struct pci_dev *pci)
{
    struct snd_card *card = pci_get_drvdata(pci);
    struct motu_pci *motu = card->private_data;

    iowrite32(0, motu->iobase_reg + 0x24);

    snd_card_free(card);
    free_irq(motu->irq, motu);
    pci_iounmap(pci, motu->iobase_port);
    pci_iounmap(pci, motu->iobase_reg);
    pci_iounmap(pci, motu->iobase_dsp);
    pci_release_regions(pci);
    pci_disable_device(pci);
}

static const struct pci_device_id snd_motu_ids[] = {
    { PCI_DEVICE(MOTU_VENDOR_ID, MOTU_DEVICE_ID) },
    { 0, }
};
MODULE_DEVICE_TABLE(pci, snd_motu_ids);

static struct pci_driver motu_driver = {
    .name = KBUILD_MODNAME,
    .id_table = snd_motu_ids,
    .probe = snd_motu_probe,
    .remove = snd_motu_remove,
};

module_pci_driver(motu_driver);
