// SPDX-License-Identifier: GPL-2.0
/*
 * motu424_win_pal.c - Windows implementation of the Platform Abstraction
 *                     Layer for the MOTU PCI-424 driver.
 *
 * Wraps Windows kernel WDF/WDM APIs into the PAL interface.
 *
 * Build with WDK for Windows 10/11 x64.
 */

#include <ntddk.h>
#include <wdf.h>
#include <wdm.h>
#include <stdarg.h>
#include <stdlib.h>

#include "motu424_pal.h"
#include "motu424_hw.h"

/* DeviceGetContext is defined in motu424_wdf.c via WDF_DECLARE_CONTEXT_TYPE_WITH_NAME */
extern void *DeviceGetContext(void *Device);

/* ---- Logging ------------------------------------------------------------ */

void pal_log(int level, const char *fmt, ...)
{
	UNREFERENCED_PARAMETER(level);

	/* Use vDbgPrintExWithPrefix for kernel-safe formatted debug output.
	 * This is a native kernel function that doesn't depend on UCRT. */
	{
		va_list args;
		va_start(args, fmt);
		vDbgPrintExWithPrefix("motu424: ", DPFLTR_DEFAULT_ID,
				      DPFLTR_INFO_LEVEL, fmt, args);
		va_end(args);
	}
}

/* ---- Timing ------------------------------------------------------------- */

void pal_udelay(unsigned int usecs)
{
	/* KeStallExecutionProcessor is for IRQL <= DISPATCH_LEVEL
	 * and stalls the processor for the given microseconds */
	KeStallExecutionProcessor((ULONG)usecs);
}

void pal_msleep(unsigned int msecs)
{
	LARGE_INTEGER interval;
	/* Negative = relative time in 100ns units */
	interval.QuadPart = -10000LL * (LONGLONG)msecs;
	KeDelayExecutionThread(KernelMode, FALSE, &interval);
}

void pal_cond_resched(void)
{
	/* At PASSIVE_LEVEL we can yield; at DISPATCH_LEVEL we can't */
	if (KeGetCurrentIrql() <= PASSIVE_LEVEL) {
		LARGE_INTEGER zero = { .QuadPart = 0 };
		KeDelayExecutionThread(KernelMode, FALSE, &zero);
	}
}

/* ---- Spinlock ----------------------------------------------------------- */

struct pal_spinlock {
	KSPIN_LOCK lock;
};

struct pal_spinlock *pal_spinlock_create(void)
{
	struct pal_spinlock *lock;

	/* Non-paged pool for spinlocks (must be accessible at DISPATCH_LEVEL) */
	lock = (struct pal_spinlock *)ExAllocatePool2(
		POOL_FLAG_NON_PAGED, sizeof(*lock), 'MOTU');
	if (lock) {
		KeInitializeSpinLock(&lock->lock);
	}
	return lock;
}

void pal_spinlock_destroy(struct pal_spinlock *lock)
{
	if (lock)
		ExFreePool(lock);
}

void pal_spinlock_lock_irqsave(struct pal_spinlock *lock,
			       unsigned long *flags)
{
	KIRQL irql;
	KeAcquireSpinLock(&lock->lock, &irql);
	*flags = (unsigned long)irql;
}

void pal_spinlock_unlock_irqrestore(struct pal_spinlock *lock,
				    unsigned long flags)
{
	KeReleaseSpinLock(&lock->lock, (KIRQL)flags);
}

/* ---- PCI ---------------------------------------------------------------- */

/* On Windows, PCI enable/master/DMA mask are handled by WDF PnP.
 * The PAL device struct stores the WDF device handle and resource info. */

int pal_pci_enable(struct pal_device *dev)
{
	UNREFERENCED_PARAMETER(dev);
	/* WDF handles PCI enablement in EvtDevicePrepareHardware */
	return 0;
}

void pal_pci_disable(struct pal_device *dev)
{
	UNREFERENCED_PARAMETER(dev);
	/* WDF handles this in EvtDeviceReleaseHardware */
}

int pal_pci_set_master(struct pal_device *dev)
{
	UNREFERENCED_PARAMETER(dev);
	/* Bus mastering is enabled by WDF DMA enabler */
	return 0;
}

int pal_pci_set_dma_mask(struct pal_device *dev, int bits)
{
	UNREFERENCED_PARAMETER(dev);
	UNREFERENCED_PARAMETER(bits);
	/* WDF DMA enabler handles this. We use 32-bit common buffer. */
	return 0;
}

/* ---- BAR mapping -------------------------------------------------------- */
/*
 * On Windows, BAR mapping is done in EvtDevicePrepareHardware using
 * MmMapIoSpaceEx on the translated CmResourceList entries. The mapped
 * addresses are stored in the device context and then set into the
 * motu424_ctx iobase fields by the WDF driver before calling shared code.
 *
 * pal_iomap is a no-op on Windows — the WDF driver handles this directly.
 */

void *pal_iomap(struct pal_device *dev, int bar)
{
	/* Not used on Windows — WDF driver maps resources directly */
	UNREFERENCED_PARAMETER(dev);
	UNREFERENCED_PARAMETER(bar);
	return NULL;
}

void pal_iounmap(struct pal_device *dev, void *addr)
{
	/* WDF driver unmaps in EvtDeviceReleaseHardware */
	UNREFERENCED_PARAMETER(dev);
	UNREFERENCED_PARAMETER(addr);
}

/* ---- MMIO --------------------------------------------------------------- */

void pal_write32(void *base, uint32_t offset, uint32_t value)
{
	WRITE_REGISTER_ULONG(
		(volatile ULONG *)((ULONG_PTR)base + offset),
		(ULONG)value);
}

uint32_t pal_read32(void *base, uint32_t offset)
{
	return (uint32_t)READ_REGISTER_ULONG(
		(volatile ULONG *)((ULONG_PTR)base + offset));
}

/* ---- DMA ---------------------------------------------------------------- */
/*
 * On Windows, DMA uses WDF common buffers. The WDF driver creates a
 * WDFDMAENABLER in EvtDeviceAdd, then pal_dma_alloc creates a
 * WDFCOMMONBUFFER from that enabler.
 */

int pal_dma_alloc(struct pal_device *dev, size_t size, void **vaddr,
		  uint64_t *paddr)
{
	WDF_COMMON_BUFFER_CONFIG config;
	WDFCOMMONBUFFER buffer;
	NTSTATUS status;

	if (!dev || !dev->dma_enabler) {
		pal_log(PAL_LOG_ERR, "DMA enabler not initialized\n");
		return -1;
	}

	WDF_COMMON_BUFFER_CONFIG_INIT(&config, (ULONG)size);

	status = WdfCommonBufferCreate(dev->dma_enabler, (ULONG)size,
				      WDF_NO_OBJECT_ATTRIBUTES, &buffer);
	if (!NT_SUCCESS(status)) {
		pal_log(PAL_LOG_ERR, "WdfCommonBufferCreate failed: 0x%x\n",
			status);
		return -1;
	}

	*vaddr = WdfCommonBufferGetAlignedVirtualAddress(buffer);
	*paddr = (uint64_t)WdfCommonBufferGetAlignedLogicalAddress(buffer).QuadPart;

	/* Store the buffer handle for later free */
	dev->dma_buffer = buffer;

	pal_log(PAL_LOG_INFO, "DMA allocated: vaddr=%p paddr=0x%llx size=%zu\n",
		*vaddr, (unsigned long long)*paddr, size);

	return 0;
}

void pal_dma_free(struct pal_device *dev, void *vaddr, uint64_t paddr,
		  size_t size)
{
	UNREFERENCED_PARAMETER(vaddr);
	UNREFERENCED_PARAMETER(paddr);
	UNREFERENCED_PARAMETER(size);

	if (dev && dev->dma_buffer) {
		WdfObjectDelete(dev->dma_buffer);
		dev->dma_buffer = NULL;
	}
}

/* ---- IRQ ---------------------------------------------------------------- */

static BOOLEAN pal_win_irq_handler(WDFINTERRUPT interrupt, ULONG messageID)
{
	WDFDEVICE device;
	struct pal_device *dev;

	UNREFERENCED_PARAMETER(messageID);

	/* Get the device from the interrupt, then get our context */
	device = WdfInterruptGetDevice(interrupt);
	dev = (struct pal_device *)DeviceGetContext(device);
	if (dev && dev->irq_handler)
		dev->irq_handler(dev->irq_ctx);

	return TRUE;
}

static NTSTATUS pal_win_irq_enable(WDFINTERRUPT interrupt,
				   WDFDEVICE associatedDevice)
{
	UNREFERENCED_PARAMETER(associatedDevice);
	UNREFERENCED_PARAMETER(interrupt);
	return STATUS_SUCCESS;
}

static NTSTATUS pal_win_irq_disable(WDFINTERRUPT interrupt,
				    WDFDEVICE associatedDevice)
{
	UNREFERENCED_PARAMETER(interrupt);
	UNREFERENCED_PARAMETER(associatedDevice);
	return STATUS_SUCCESS;
}

int pal_irq_request(struct pal_device *dev, pal_irq_handler_t handler,
		    void *ctx)
{
	WDF_INTERRUPT_CONFIG config;
	WDFINTERRUPT interrupt;
	NTSTATUS status;

	dev->irq_handler = handler;
	dev->irq_ctx = ctx;

	WDF_INTERRUPT_CONFIG_INIT(&config, pal_win_irq_handler, NULL);
	config.EvtInterruptEnable = pal_win_irq_enable;
	config.EvtInterruptDisable = pal_win_irq_disable;

	status = WdfInterruptCreate((WDFDEVICE)dev->wdf_device, &config,
				    WDF_NO_OBJECT_ATTRIBUTES, &interrupt);
	if (!NT_SUCCESS(status)) {
		pal_log(PAL_LOG_ERR, "WdfInterruptCreate failed: 0x%x\n",
			status);
		return -1;
	}

	dev->interrupt = (void *)interrupt;
	return 0;
}

void pal_irq_free(struct pal_device *dev)
{
	/* WDF handles interrupt cleanup when the device is removed */
	dev->irq_handler = NULL;
	dev->irq_ctx = NULL;
}

/* ---- Firmware ----------------------------------------------------------- */
/*
 * On Windows, firmware files are loaded from the driver directory
 * (%SystemRoot%\System32\drivers\). The WDF driver reads them using
 * kernel file I/O and passes the data to the shared core.
 *
 * pal_firmware_load is a helper that reads a file from the driver's
 * directory. The WDF driver calls this and passes the result to
 * motu424_full_init().
 */

int pal_firmware_load(struct pal_device *dev, const char *name,
		      const void **data, size_t *size)
{
	UNREFERENCED_PARAMETER(dev);
	UNREFERENCED_PARAMETER(name);
	UNREFERENCED_PARAMETER(data);
	UNREFERENCED_PARAMETER(size);

	/* The WDF driver handles firmware loading directly using
	 * ZwCreateFile/ZwReadFile. This PAL function is a placeholder. */
	pal_log(PAL_LOG_WARN, "pal_firmware_load: not implemented on Windows\n");
	return -1;
}

void pal_firmware_free(const void *data)
{
	UNREFERENCED_PARAMETER(data);
	/* WDF driver manages firmware buffer lifecycle */
}
