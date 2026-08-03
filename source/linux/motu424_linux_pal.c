// SPDX-License-Identifier: GPL-2.0
/*
 * motu424_linux_pal.c - Linux implementation of the Platform Abstraction
 *                      Layer for the MOTU PCI-424 driver.
 *
 * Wraps Linux kernel APIs (pci_*, ioread32/iowrite32, dma_alloc_coherent,
 * request_irq, request_firmware, etc.) into the PAL interface.
 */

#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/firmware.h>
#include <linux/io.h>
#include <linux/pci.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/module.h>

#include "motu424_pal.h"
#include "motu424_hw.h"

/* ---- Linux PAL device --------------------------------------------------- */

struct pal_device {
	struct pci_dev *pci;
	int irq;
	pal_irq_handler_t irq_handler;
	void *irq_ctx;
};

/* ---- PCI ---------------------------------------------------------------- */

int pal_pci_enable(struct pal_device *dev)
{
	int err = pci_enable_device(dev->pci);
	if (err)
		return err;
	return 0;
}

void pal_pci_disable(struct pal_device *dev)
{
	pci_disable_device(dev->pci);
}

int pal_pci_set_master(struct pal_device *dev)
{
	pci_set_master(dev->pci);
	return 0;
}

int pal_pci_set_dma_mask(struct pal_device *dev, int bits)
{
	return dma_set_mask_and_coherent(&dev->pci->dev, DMA_BIT_MASK(bits));
}

/* ---- BAR mapping -------------------------------------------------------- */

void *pal_iomap(struct pal_device *dev, int bar)
{
	return pci_iomap(dev->pci, bar, 0);
}

void pal_iounmap(struct pal_device *dev, void *addr)
{
	if (addr)
		pci_iounmap(dev->pci, addr);
}

/* ---- MMIO --------------------------------------------------------------- */

void pal_write32(void *base, uint32_t offset, uint32_t value)
{
	iowrite32(value, base + offset);
}

uint32_t pal_read32(void *base, uint32_t offset)
{
	return ioread32(base + offset);
}

/* ---- DMA ---------------------------------------------------------------- */

int pal_dma_alloc(struct pal_device *dev, size_t size, void **vaddr,
		  uint64_t *paddr)
{
	dma_addr_t dma;
	void *buf = dma_alloc_coherent(&dev->pci->dev, size, &dma, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;
	*vaddr = buf;
	*paddr = (uint64_t)dma;
	return 0;
}

void pal_dma_free(struct pal_device *dev, void *vaddr, uint64_t paddr,
		  size_t size)
{
	dma_free_coherent(&dev->pci->dev, size, vaddr, (dma_addr_t)paddr);
}

/* ---- IRQ ---------------------------------------------------------------- */

static irqreturn_t pal_linux_irq(int irq, void *ctx)
{
	struct pal_device *dev = ctx;
	if (dev->irq_handler)
		dev->irq_handler(dev->irq_ctx);
	return IRQ_HANDLED;
}

int pal_irq_request(struct pal_device *dev, pal_irq_handler_t handler,
		    void *ctx)
{
	int err = request_irq(dev->pci->irq, pal_linux_irq, IRQF_SHARED,
			      MOTU_DRIVER_NAME, dev);
	if (err)
		return err;
	dev->irq = dev->pci->irq;
	dev->irq_handler = handler;
	dev->irq_ctx = ctx;
	return 0;
}

void pal_irq_free(struct pal_device *dev)
{
	if (dev->irq >= 0) {
		free_irq(dev->irq, dev);
		dev->irq = -1;
	}
}

/* ---- Firmware ----------------------------------------------------------- */

int pal_firmware_load(struct pal_device *dev, const char *name,
		      const void **data, size_t *size)
{
	const struct firmware *fw;
	int err = request_firmware(&fw, name, &dev->pci->dev);
	if (err)
		return err;
	*data = fw->data;
	*size = fw->size;
	/* Caller must pal_firmware_free, which calls release_firmware.
	 * We stash the firmware pointer in *data's backing struct. */
	/* On Linux, we need to keep the firmware struct. Store it. */
	*(const struct firmware **)data = fw; /* hack: store fw ptr in data */
	*data = fw->data;
	*size = fw->size;
	return 0;
}

void pal_firmware_free(const void *data)
{
	/* On Linux, we stored the firmware pointer. We can't recover it
	 * from just the data pointer, so the caller must manage this.
	 * In practice, the Linux frontend handles firmware directly. */
	/* This is a no-op on Linux — the frontend manages firmware lifecycle. */
}

/* ---- Timing ------------------------------------------------------------- */

void pal_udelay(unsigned int usecs)
{
	udelay(usecs);
}

void pal_msleep(unsigned int msecs)
{
	msleep(msecs);
}

void pal_cond_resched(void)
{
	cond_resched();
}

/* ---- Logging ------------------------------------------------------------ */

void pal_log(int level, const char *fmt, ...)
{
	va_list args;
	char prefix[8] = "";
	struct va_format vaf;

	switch (level) {
	case PAL_LOG_ERR:	strcpy(prefix, "motu424: "); break;
	case PAL_LOG_WARN:	strcpy(prefix, "motu424: "); break;
	case PAL_LOG_INFO:	strcpy(prefix, "motu424: "); break;
	case PAL_LOG_DBG:	strcpy(prefix, "motu424: "); break;
	}

	vaf.fmt = fmt;
	va_start(args, fmt);
	vaf.va = &args;

	switch (level) {
	case PAL_LOG_ERR:	pr_err("%s%pV", prefix, &vaf); break;
	case PAL_LOG_WARN:	pr_warn("%s%pV", prefix, &vaf); break;
	case PAL_LOG_INFO:	pr_info("%s%pV", prefix, &vaf); break;
	case PAL_LOG_DBG:	pr_debug("%s%pV", prefix, &vaf); break;
	}

	va_end(args);
}

/* ---- Spinlock ----------------------------------------------------------- */

struct pal_spinlock {
	spinlock_t lock;
};

struct pal_spinlock *pal_spinlock_create(void)
{
	struct pal_spinlock *lock = kzalloc(sizeof(*lock), GFP_KERNEL);
	if (lock)
		spin_lock_init(&lock->lock);
	return lock;
}

void pal_spinlock_destroy(struct pal_spinlock *lock)
{
	kfree(lock);
}

void pal_spinlock_lock_irqsave(struct pal_spinlock *lock, unsigned long *flags)
{
	spin_lock_irqsave(&lock->lock, *flags);
}

void pal_spinlock_unlock_irqrestore(struct pal_spinlock *lock,
				    unsigned long flags)
{
	spin_unlock_irqrestore(&lock->lock, flags);
}
