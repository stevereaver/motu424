/* SPDX-License-Identifier: GPL-2.0 */
/*
 * motu424_pal.h - Platform Abstraction Layer for the MOTU PCI-424 driver.
 *
 * Defines the OS-agnostic interface that the shared hardware logic calls.
 * Each platform (Linux, Windows) provides its own implementation.
 *
 * The PAL abstracts:
 *   - PCI device enable and BAR mapping
 *   - MMIO register read/write
 *   - DMA buffer allocation
 *   - Interrupt request/free
 *   - Firmware loading
 *   - Timing/delay
 *   - Logging
 */

#ifndef _MOTU424_PAL_H
#define _MOTU424_PAL_H

/* MSVC compatibility shims */
#ifdef _MSC_VER
#include <stdint.h>
#define __packed
#define __attribute_format_printf(fmt, arg)
#else
#include <stdint.h>
#define __packed __attribute__((packed))
#define __attribute_format_printf(fmt, arg) __attribute__((format(printf, fmt, arg)))
#endif

/* ---- PAL types ---------------------------------------------------------- */

/* IRQ handler function type (forward declaration for pal_device) */
typedef void (*pal_irq_handler_t)(void *ctx);

/* Opaque device handle — platform-specific PCI device wrapper.
 * On Linux this wraps a struct pci_dev.
 * On Windows this wraps a WDFDEVICE with DMA enabler and interrupt. */
struct pal_device {
#ifdef _MSC_VER
	/* Windows WDF device handle */
	void			*wdf_device;	/* WDFDEVICE */
	void			*dma_enabler;	/* WDFDMAENABLER */
	void			*dma_buffer;	/* WDFCOMMONBUFFER */
	void			*interrupt;	/* WDFINTERRUPT */
#endif
	pal_irq_handler_t	irq_handler;
	void			*irq_ctx;
};

/* BAR region indices */
#define PAL_BAR_DSP	0	/* 4 MB prefetchable - DSP memory   */
#define PAL_BAR_REG	1	/* 8 MB non-prefetch - registers    */
#define PAL_BAR_PORT	2	/* 16 bytes I/O - FPGA config port   */

/* Log levels */
#define PAL_LOG_ERR	0
#define PAL_LOG_WARN	1
#define PAL_LOG_INFO	2
#define PAL_LOG_DBG	3

/* ---- PAL interface ------------------------------------------------------ */

/* PCI device */
int  pal_pci_enable(struct pal_device *dev);
void pal_pci_disable(struct pal_device *dev);
int  pal_pci_set_master(struct pal_device *dev);
int  pal_pci_set_dma_mask(struct pal_device *dev, int bits);

/* BAR mapping — returns virtual address for MMIO access, or NULL on error */
void *pal_iomap(struct pal_device *dev, int bar);
void  pal_iounmap(struct pal_device *dev, void *addr);

/* MMIO access */
void     pal_write32(void *base, uint32_t offset, uint32_t value);
uint32_t pal_read32(void *base, uint32_t offset);

/* DMA — allocate coherent (consistent) buffer */
int  pal_dma_alloc(struct pal_device *dev, size_t size,
		   void **vaddr, uint64_t *paddr);
void pal_dma_free(struct pal_device *dev, void *vaddr, uint64_t paddr,
		  size_t size);

/* IRQ */
int  pal_irq_request(struct pal_device *dev, pal_irq_handler_t handler,
		     void *ctx);
void pal_irq_free(struct pal_device *dev);

/* Firmware loading */
int  pal_firmware_load(struct pal_device *dev, const char *name,
		       const void **data, size_t *size);
void pal_firmware_free(const void *data);

/* Timing */
void pal_udelay(unsigned int usecs);
void pal_msleep(unsigned int msecs);
void pal_cond_resched(void);

/* Logging */
void pal_log(int level, const char *fmt, ...)
	__attribute_format_printf(2, 3);

/* Spinlock — minimal lock abstraction for IRQ-safe locking */
struct pal_spinlock;
struct pal_spinlock *pal_spinlock_create(void);
void pal_spinlock_destroy(struct pal_spinlock *lock);
void pal_spinlock_lock_irqsave(struct pal_spinlock *lock,
			       unsigned long *flags);
void pal_spinlock_unlock_irqrestore(struct pal_spinlock *lock,
				    unsigned long flags);

/* ---- Device context ----------------------------------------------------- */

/*
 * The shared driver context. Contains BAR mappings and state that is
 * common across platforms. Platform-specific extensions are stored
 * in the pal_device itself.
 */
struct motu424_ctx {
	struct pal_device	*dev;

	/* BAR mappings (virtual addresses) */
	void			*iobase_dsp;	/* BAR0 - DSP memory     */
	void			*iobase_reg;	/* BAR1 - control regs   */
	void			*iobase_port;	/* BAR2 - FPGA config port */

	/* DMA */
	void			*dma_buf;	/* coherent DMA buffer   */
	uint64_t		dma_addr;	/* physical address      */
	size_t			dma_size;	/* buffer size in bytes  */

	/* IRQ */
	int			irq_registered;

	/* Lock */
	struct pal_spinlock	*lock;
	unsigned long		lock_flags;	/* saved flags */

	/* Hardware state */
	uint32_t		port_conf_shadow;
	unsigned int		rate;

	/* Audio state — platform fills these in */
	void			*playback_ctx;
	void			*capture_ctx;
	int			dma_running;
	uint64_t		start_time_ns;
};

/* ---- Shared inline MMIO helpers (use PAL) ------------------------------- */

static inline void motu_write(struct motu424_ctx *ctx, uint8_t bar,
			      uint32_t offset, uint32_t value)
{
	switch (bar) {
	case PAL_BAR_DSP:
		pal_write32(ctx->iobase_dsp, offset, value);
		break;
	case PAL_BAR_REG:
		pal_write32(ctx->iobase_reg, offset, value);
		break;
	case PAL_BAR_PORT:
		pal_write32(ctx->iobase_port, offset, value);
		break;
	}
}

static inline uint32_t motu_read(struct motu424_ctx *ctx, uint8_t bar,
				 uint32_t offset)
{
	switch (bar) {
	case PAL_BAR_DSP:
		return pal_read32(ctx->iobase_dsp, offset);
	case PAL_BAR_REG:
		return pal_read32(ctx->iobase_reg, offset);
	case PAL_BAR_PORT:
		return pal_read32(ctx->iobase_port, offset);
	default:
		return 0xffffffff;
	}
}

/* ---- Shared function prototypes ----------------------------------------- */

/* motu424_fpga.c — shared */
int motu424_load_fpga(struct motu424_ctx *ctx, const void *fw_data,
		      size_t fw_size);

/* motu424_init.c — shared */
int motu424_replay_sequence(struct motu424_ctx *ctx, const void *seq_data,
			    size_t seq_size);
int motu424_hw_init(struct motu424_ctx *ctx);
void motu424_hw_stop(struct motu424_ctx *ctx);
int motu424_poll_reg(struct motu424_ctx *ctx, uint8_t bar, uint32_t offset,
		     uint32_t expected, int timeout_ms);

/* Full initialization: FPGA load + DSP zero + replay + kick-start + port loop */
int motu424_full_init(struct motu424_ctx *ctx,
		      const void *fpga_fw, size_t fpga_fw_size,
		      const void *init_seq, size_t init_seq_size);

/* motu424_dma.c — shared */
int motu424_dma_alloc(struct motu424_ctx *ctx, size_t size);
void motu424_dma_free(struct motu424_ctx *ctx);
int motu424_dma_setup_sg(struct motu424_ctx *ctx);

#endif /* _MOTU424_PAL_H */
