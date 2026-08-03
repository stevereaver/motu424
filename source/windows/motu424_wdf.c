// SPDX-License-Identifier: GPL-2.0
/*
 * motu424_wdf.c - Windows WDF (Windows Driver Framework) driver for the
 *                MOTU PCI-424 audio interface.
 *
 * This is the Windows equivalent of the Linux ALSA frontend. It handles:
 *   - Driver entry (DriverEntry)
 *   - Device creation (EvtDriverDeviceAdd)
 *   - Resource mapping (EvtDevicePrepareHardware)
 *   - Hardware initialization (calls shared core via PAL)
 *   - IOCTL interface for userspace audio bridge
 *
 * Phase 1: WDF + custom IOCTL (this file)
 * Phase 2: PortCls/WaveRT miniport (future)
 *
 * Build with WDK for Windows 10/11 x64.
 */

#include <ntddk.h>
#include <wdf.h>
#include <wdm.h>

#include "motu424_pal.h"
#include "motu424_hw.h"

/* ---- IOCTL definitions -------------------------------------------------- */

#define MOTU424_DEVICE_TYPE	FILE_DEVICE_UNKNOWN
#define MOTU424_IOCTL(Function) \
	CTL_CODE(MOTU424_DEVICE_TYPE, Function, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_MOTU424_GET_INFO	MOTU424_IOCTL(0x800)
#define IOCTL_MOTU424_START	MOTU424_IOCTL(0x801)
#define IOCTL_MOTU424_STOP	MOTU424_IOCTL(0x802)
#define IOCTL_MOTU424_GET_POSITION MOTU424_IOCTL(0x803)
#define IOCTL_MOTU424_SET_RATE	MOTU424_IOCTL(0x804)
#define IOCTL_MOTU424_READ_REG	MOTU424_IOCTL(0x805)
#define IOCTL_MOTU424_WRITE_REG	MOTU424_IOCTL(0x806)
#define IOCTL_MOTU424_GET_STATUS MOTU424_IOCTL(0x807)

/* IOCTL data structures (shared with userspace test tool) */
#pragma pack(push, 1)
typedef struct _MOTU424_INFO {
	uint32_t	rate;
	uint32_t	dma_running;
	uint32_t	port_status;	/* BAR2 0x00 */
	uint32_t	port_conf;	/* BAR1 0x00 */
} MOTU424_INFO, *PMOTU424_INFO;

typedef struct _MOTU424_REG_OP {
	uint32_t	bar;
	uint32_t	offset;
	uint32_t	value;
} MOTU424_REG_OP, *PMOTU424_REG_OP;
#pragma pack(pop)

/* ---- Device context ----------------------------------------------------- */

typedef struct _DEVICE_CONTEXT {
	struct pal_device	pal_dev;
	struct motu424_ctx	ctx;

	/* BAR mappings (translated resources from PnP) */
	void			*bar0_base;	/* BAR0 - DSP memory */
	ULONG			bar0_len;
	void			*bar1_base;	/* BAR1 - Control registers */
	ULONG			bar1_len;
	void			*bar2_base;	/* BAR2 - FPGA config port */
	ULONG			bar2_len;

	/* WDF DMA enabler */
	WDFDMAENABLER		dma_enabler;

	/* Firmware data (loaded from driver directory) */
	void			*fw_fpga_data;
	size_t			fw_fpga_size;
	void			*fw_init_data;
	size_t			fw_init_size;

} DEVICE_CONTEXT, *PDEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, DeviceGetContext)

/* ---- Forward declarations ------------------------------------------------ */

static NTSTATUS EvtDriverDeviceAdd(WDFDRIVER, PWDFDEVICE_INIT);
static NTSTATUS EvtDevicePrepareHardware(WDFDEVICE, WDFCMRESLIST,
					 WDFCMRESLIST);
static NTSTATUS EvtDeviceReleaseHardware(WDFDEVICE, WDFCMRESLIST);
static VOID EvtDeviceContextCleanup(WDFOBJECT);
static NTSTATUS EvtIoDeviceControl(WDFQUEUE, WDFREQUEST, size_t,
				   size_t, ULONG);
static VOID EvtIoDefault(WDFQUEUE, WDFREQUEST);

/* ---- IRQ handler (called by PAL interrupt ISR) --------------------------- */

static void motu424_irq_callback(void *ctx_data)
{
	PDEVICE_CONTEXT ctx = (PDEVICE_CONTEXT)ctx_data;
	uint32_t status;

	if (!ctx->ctx.iobase_reg)
		return;

	status = pal_read32(ctx->ctx.iobase_reg, MOTU_REG_INT_STATUS);
	if (!(status & MOTU_INT_PERIOD_ELAPSED))
		return;

	/* Acknowledge */
	pal_write32(ctx->ctx.iobase_reg, MOTU_REG_INT_ACK, status);
}

/* ---- Firmware loading from driver directory ------------------------------- */
/*
 * Reads a file from %SystemRoot%\System32\drivers\ (where the .sys is
 * installed) into a non-paged buffer.
 */

static NTSTATUS LoadFirmwareFile(PDEVICE_CONTEXT ctx, const wchar_t *filename,
				 void **data, size_t *size)
{
	UNICODE_STRING name;
	OBJECT_ATTRIBUTES attrs;
	IO_STATUS_BLOCK iosb;
	HANDLE hFile;
	NTSTATUS status;
	FILE_STANDARD_INFORMATION fsi;
	LARGE_INTEGER byteOffset;

	/* Build full path: \SystemRoot\System32\drivers\<filename> */
	RtlInitUnicodeString(&name, filename);

	InitializeObjectAttributes(&attrs, &name,
		OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
		NULL, NULL);

	/* Open the file */
	status = ZwCreateFile(&hFile, GENERIC_READ | SYNCHRONIZE, &attrs,
		&iosb, NULL, FILE_ATTRIBUTE_NORMAL, FILE_SHARE_READ,
		FILE_OPEN, FILE_SYNCHRONOUS_IO_NONALERT, NULL, 0);
	if (!NT_SUCCESS(status)) {
		pal_log(PAL_LOG_WARN, "Firmware file not found: %ws (0x%x)\n",
			filename, status);
		return status;
	}

	/* Get file size */
	status = ZwQueryInformationFile(hFile, &iosb, &fsi,
		sizeof(fsi), FileStandardInformation);
	if (!NT_SUCCESS(status)) {
		pal_log(PAL_LOG_ERR, "ZwQueryInformationFile failed: 0x%x\n",
			status);
		ZwClose(hFile);
		return status;
	}

	*size = (size_t)fsi.EndOfFile.QuadPart;

	/* Allocate non-paged buffer */
	*data = ExAllocatePool2(POOL_FLAG_NON_PAGED, (SIZE_T)*size, 'MOTU');
	if (!*data) {
		pal_log(PAL_LOG_ERR, "Failed to allocate %zu bytes for fw\n",
			*size);
		ZwClose(hFile);
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	/* Read the file */
	byteOffset.QuadPart = 0;
	status = ZwReadFile(hFile, NULL, NULL, NULL, &iosb, *data,
		(SIZE_T)*size, &byteOffset, NULL);
	ZwClose(hFile);

	if (!NT_SUCCESS(status)) {
		pal_log(PAL_LOG_ERR, "ZwReadFile failed: 0x%x\n", status);
		ExFreePool(*data);
		*data = NULL;
		return status;
	}

	pal_log(PAL_LOG_INFO, "Loaded firmware: %ws (%zu bytes)\n",
		filename, *size);
	return STATUS_SUCCESS;
}

/* ---- DriverEntry -------------------------------------------------------- */

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
	WDF_DRIVER_CONFIG config;
	NTSTATUS status;

	pal_log(PAL_LOG_INFO, "MOTU PCI-424 WDF driver loading\n");

	WDF_DRIVER_CONFIG_INIT(&config, EvtDriverDeviceAdd);
	config.DriverPoolTag = 'MOTU';

	status = WdfDriverCreate(DriverObject, RegistryPath,
				WDF_NO_OBJECT_ATTRIBUTES, &config, NULL);
	if (!NT_SUCCESS(status)) {
		pal_log(PAL_LOG_ERR, "WdfDriverCreate failed: 0x%x\n", status);
		return status;
	}

	return STATUS_SUCCESS;
}

/* ---- EvtDriverDeviceAdd -------------------------------------------------- */

static NTSTATUS EvtDriverDeviceAdd(WDFDRIVER Driver, PWDFDEVICE_INIT DeviceInit)
{
	NTSTATUS status;
	WDFDEVICE device;
	WDF_OBJECT_ATTRIBUTES attrs;
	WDF_PNPPOWER_EVENT_CALLBACKS pnpCallbacks;
	WDF_IO_QUEUE_CONFIG queueConfig;
	WDF_DMA_ENABLER_CONFIG dmaConfig;
	PDEVICE_CONTEXT ctx;

	UNREFERENCED_PARAMETER(Driver);

	pal_log(PAL_LOG_INFO, "EvtDriverDeviceAdd\n");

	/* Set up device context */
	WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attrs, DEVICE_CONTEXT);
	attrs.EvtCleanupCallback = EvtDeviceContextCleanup;

	/* Set PnP power callbacks */
	WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpCallbacks);
	pnpCallbacks.EvtDevicePrepareHardware = EvtDevicePrepareHardware;
	pnpCallbacks.EvtDeviceReleaseHardware = EvtDeviceReleaseHardware;

	WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &pnpCallbacks);

	/* Set device type for IOCTL */
	WdfDeviceInitSetIoType(DeviceInit, WdfDeviceIoBuffered);

	/* Create the device */
	status = WdfDeviceCreate(&DeviceInit, &attrs, &device);
	if (!NT_SUCCESS(status)) {
		pal_log(PAL_LOG_ERR, "WdfDeviceCreate failed: 0x%x\n", status);
		return status;
	}

	ctx = DeviceGetContext(device);
	RtlZeroMemory(ctx, sizeof(*ctx));
	ctx->pal_dev.wdf_device = device;
	ctx->ctx.dev = &ctx->pal_dev;
	ctx->ctx.rate = MOTU_RATE_48000;

	/* Create I/O queue for IOCTL handling */
	WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queueConfig,
					       WdfIoQueueDispatchSequential);
	queueConfig.EvtIoDeviceControl = EvtIoDeviceControl;

	status = WdfIoQueueCreate(device, &queueConfig,
				  WDF_NO_OBJECT_ATTRIBUTES, NULL);
	if (!NT_SUCCESS(status)) {
		pal_log(PAL_LOG_ERR, "WdfIoQueueCreate failed: 0x%x\n", status);
		return status;
	}

	/* Create DMA enabler for 32-bit bus-master DMA */
	WDF_DMA_ENABLER_CONFIG_INIT(&dmaConfig, WdfDmaProfileScatterGather64,
				    MOTU_DMA_BUF_MAX);
	status = WdfDmaEnablerCreate(device, &dmaConfig,
				    WDF_NO_OBJECT_ATTRIBUTES,
				    &ctx->dma_enabler);
	if (!NT_SUCCESS(status)) {
		pal_log(PAL_LOG_WARN, "WdfDmaEnablerCreate failed: 0x%x\n",
			status);
		/* Non-fatal — we can still do PIO */
	} else {
		ctx->pal_dev.dma_enabler = ctx->dma_enabler;
	}

	/* Create spinlock */
	ctx->ctx.lock = pal_spinlock_create();

	pal_log(PAL_LOG_INFO, "Device added successfully\n");
	return STATUS_SUCCESS;
}

/* ---- EvtDevicePrepareHardware -------------------------------------------- */

static NTSTATUS EvtDevicePrepareHardware(WDFDEVICE Device,
					 WDFCMRESLIST Resources,
					 WDFCMRESLIST ResourcesTranslated)
{
	PDEVICE_CONTEXT ctx = DeviceGetContext(Device);
	ULONG count = WdfCmResourceListGetCount(ResourcesTranslated);
	ULONG i;
	NTSTATUS status = STATUS_SUCCESS;
	ULONG bar_index = 0;

	UNREFERENCED_PARAMETER(Resources);

	pal_log(PAL_LOG_INFO, "PrepareHardware: %lu translated resources\n",
		count);

	/* Parse translated resources to find BAR mappings and IRQ */
	for (i = 0; i < count; i++) {
		PCM_PARTIAL_RESOURCE_DESCRIPTOR desc =
			WdfCmResourceListGetDescriptor(ResourcesTranslated, i);

		if (!desc)
			continue;

		switch (desc->Type) {
		case CmResourceTypeMemory: {
			/* Memory-mapped BAR region */
			void *base = MmMapIoSpaceEx(
				desc->u.Memory.Start,
				desc->u.Memory.Length,
				PAGE_READWRITE | PAGE_NOCACHE);

			if (!base) {
				pal_log(PAL_LOG_ERR,
					"MmMapIoSpace failed for BAR (phys=0x%llx len=%lu)\n",
					(unsigned long long)
						desc->u.Memory.Start.QuadPart,
					desc->u.Memory.Length);
				return STATUS_INSUFFICIENT_RESOURCES;
			}

			pal_log(PAL_LOG_INFO,
				"BAR%d: phys=0x%llx len=%lu virt=%p\n",
				bar_index,
				(unsigned long long)
					desc->u.Memory.Start.QuadPart,
				desc->u.Memory.Length, base);

			/* Assign based on bar index */
			switch (bar_index) {
			case 0: /* BAR0 - DSP memory (4 MB) */
				ctx->bar0_base = base;
				ctx->bar0_len = desc->u.Memory.Length;
				ctx->ctx.iobase_dsp = base;
				break;
			case 1: /* BAR1 - Control registers (8 MB) */
				ctx->bar1_base = base;
				ctx->bar1_len = desc->u.Memory.Length;
				ctx->ctx.iobase_reg = base;
				break;
			case 2: /* BAR2 - FPGA config port (16 bytes) */
				ctx->bar2_base = base;
				ctx->bar2_len = desc->u.Memory.Length;
				ctx->ctx.iobase_port = base;
				break;
			}
			bar_index++;
			break;
		}

		case CmResourceTypePort: {
			/* I/O port BAR (BAR2 may be I/O port on some configs) */
			pal_log(PAL_LOG_INFO,
				"I/O port BAR: phys=0x%llx len=%lu\n",
				(unsigned long long)
					desc->u.Port.Start.QuadPart,
				desc->u.Port.Length);
			/* For I/O ports, we use the physical address directly
			 * with READ_PORT_ULONG / WRITE_PORT_ULONG, but our PAL
			 * uses memory-mapped access. If BAR2 is I/O port,
			 * we'd need special handling. For now, assume MMIO. */
			break;
		}

		case CmResourceTypeInterrupt:
			pal_log(PAL_LOG_INFO, "IRQ resource found\n");
			break;
		}
	}

	/* Check we have all required BARs */
	if (!ctx->ctx.iobase_dsp || !ctx->ctx.iobase_reg) {
		pal_log(PAL_LOG_ERR,
			"Missing required BARs (dsp=%p reg=%p)\n",
			ctx->ctx.iobase_dsp, ctx->ctx.iobase_reg);
		return STATUS_DEVICE_CONFIGURATION_ERROR;
	}

	/* Allocate DMA buffer */
	{
		int err = motu424_dma_alloc(&ctx->ctx, MOTU_DMA_BUF_MAX);
		if (err) {
			pal_log(PAL_LOG_WARN, "DMA alloc failed (continuing)\n");
		}
	}

	/* Load firmware files from driver directory */
	{
		/* Try to load from \SystemRoot\system32\drivers\motu424\ */
		status = LoadFirmwareFile(ctx,
			L"\\SystemRoot\\System32\\drivers\\motu424\\altera424b.rbf",
			&ctx->fw_fpga_data, &ctx->fw_fpga_size);
		if (!NT_SUCCESS(status)) {
			pal_log(PAL_LOG_WARN,
				"FPGA firmware not found (warm boot)\n");
			ctx->fw_fpga_data = NULL;
		}

		status = LoadFirmwareFile(ctx,
			L"\\SystemRoot\\System32\\drivers\\motu424\\init_sequence.bin",
			&ctx->fw_init_data, &ctx->fw_init_size);
		if (!NT_SUCCESS(status)) {
			pal_log(PAL_LOG_WARN,
				"Init sequence not found (warm boot)\n");
			ctx->fw_init_data = NULL;
		}
	}

	/* Full hardware initialization */
	{
		int err = motu424_full_init(&ctx->ctx,
					   ctx->fw_fpga_data, ctx->fw_fpga_size,
					   ctx->fw_init_data, ctx->fw_init_size);
		if (err) {
			pal_log(PAL_LOG_WARN, "Hardware init incomplete: %d\n",
				err);
		}
	}

	/* Request IRQ */
	{
		int err = pal_irq_request(&ctx->pal_dev, motu424_irq_callback,
					  ctx);
		if (err) {
			pal_log(PAL_LOG_WARN, "IRQ request failed\n");
		} else {
			/* Enable interrupt */
			pal_write32(ctx->ctx.iobase_reg, MOTU_REG_INT_MASK,
				    MOTU_INT_PERIOD_ELAPSED);
			pal_log(PAL_LOG_INFO, "IRQ enabled\n");
		}
	}

	return STATUS_SUCCESS;
}

/* ---- EvtDeviceReleaseHardware -------------------------------------------- */

static NTSTATUS EvtDeviceReleaseHardware(WDFDEVICE Device,
					 WDFCMRESLIST Resources)
{
	PDEVICE_CONTEXT ctx = DeviceGetContext(Device);

	UNREFERENCED_PARAMETER(Resources);

	pal_log(PAL_LOG_INFO, "ReleaseHardware\n");

	/* Stop hardware */
	motu424_hw_stop(&ctx->ctx);

	/* Disable interrupts */
	if (ctx->ctx.iobase_reg)
		pal_write32(ctx->ctx.iobase_reg, MOTU_REG_INT_MASK, 0);

	/* Free IRQ */
	pal_irq_free(&ctx->pal_dev);

	/* Free DMA */
	motu424_dma_free(&ctx->ctx);

	/* Free firmware buffers */
	if (ctx->fw_fpga_data) {
		ExFreePool(ctx->fw_fpga_data);
		ctx->fw_fpga_data = NULL;
	}
	if (ctx->fw_init_data) {
		ExFreePool(ctx->fw_init_data);
		ctx->fw_init_data = NULL;
	}

	/* Unmap BARs */
	if (ctx->bar0_base) {
		MmUnmapIoSpace(ctx->bar0_base, ctx->bar0_len);
		ctx->bar0_base = NULL;
		ctx->ctx.iobase_dsp = NULL;
	}
	if (ctx->bar1_base) {
		MmUnmapIoSpace(ctx->bar1_base, ctx->bar1_len);
		ctx->bar1_base = NULL;
		ctx->ctx.iobase_reg = NULL;
	}
	if (ctx->bar2_base) {
		MmUnmapIoSpace(ctx->bar2_base, ctx->bar2_len);
		ctx->bar2_base = NULL;
		ctx->ctx.iobase_port = NULL;
	}

	return STATUS_SUCCESS;
}

/* ---- EvtDeviceContextCleanup --------------------------------------------- */

static VOID EvtDeviceContextCleanup(WDFOBJECT Device)
{
	PDEVICE_CONTEXT ctx = DeviceGetContext((WDFDEVICE)Device);

	if (ctx->ctx.lock) {
		pal_spinlock_destroy(ctx->ctx.lock);
		ctx->ctx.lock = NULL;
	}
}

/* ---- IOCTL handler ------------------------------------------------------- */

static NTSTATUS EvtIoDeviceControl(WDFQUEUE Queue, WDFREQUEST Request,
				   size_t OutputBufferLength,
				   size_t InputBufferLength,
				   ULONG IoControlCode)
{
	PDEVICE_CONTEXT ctx = DeviceGetContext(
		WdfIoQueueGetDevice(Queue));
	NTSTATUS status = STATUS_SUCCESS;
	size_t bytesReturned = 0;

	switch (IoControlCode) {
	case IOCTL_MOTU424_GET_INFO: {
		PMOTU424_INFO info;
		size_t bufSize;

		status = WdfRequestRetrieveOutputBuffer(Request,
			sizeof(MOTU424_INFO), (void **)&info, &bufSize);
		if (!NT_SUCCESS(status))
			break;

		info->rate = ctx->ctx.rate;
		info->dma_running = ctx->ctx.dma_running;
		info->port_status = ctx->ctx.iobase_port ?
			pal_read32(ctx->ctx.iobase_port, 0) : 0;
		info->port_conf = ctx->ctx.iobase_reg ?
			pal_read32(ctx->ctx.iobase_reg, MOTU_REG_PORT_CONF) : 0;

		bytesReturned = sizeof(MOTU424_INFO);
		break;
	}

	case IOCTL_MOTU424_START: {
		uint32_t val;

		if (!ctx->ctx.iobase_reg) {
			status = STATUS_DEVICE_NOT_READY;
			break;
		}
		val = pal_read32(ctx->ctx.iobase_reg, MOTU_REG_PORT_CONF);
		val |= MOTU_PORT_CONF_DMA_EN | MOTU_PORT_CONF_RUN |
		       MOTU_PORT_CONF_DMA_RD;
		pal_write32(ctx->ctx.iobase_reg, MOTU_REG_PORT_CONF, val);
		ctx->ctx.port_conf_shadow = val;
		ctx->ctx.dma_running = 1;
		pal_log(PAL_LOG_INFO, "DMA started\n");
		break;
	}

	case IOCTL_MOTU424_STOP: {
		uint32_t val;

		if (!ctx->ctx.iobase_reg) {
			status = STATUS_DEVICE_NOT_READY;
			break;
		}
		val = pal_read32(ctx->ctx.iobase_reg, MOTU_REG_PORT_CONF);
		val &= ~(MOTU_PORT_CONF_DMA_EN | MOTU_PORT_CONF_RUN |
			 MOTU_PORT_CONF_DMA_RD | MOTU_PORT_CONF_START);
		pal_write32(ctx->ctx.iobase_reg, MOTU_REG_PORT_CONF, val);
		ctx->ctx.port_conf_shadow = val;
		ctx->ctx.dma_running = 0;
		pal_log(PAL_LOG_INFO, "DMA stopped\n");
		break;
	}

	case IOCTL_MOTU424_GET_POSITION: {
		uint32_t *pos;
		size_t bufSize;

		status = WdfRequestRetrieveOutputBuffer(Request,
			sizeof(uint32_t), (void **)&pos, &bufSize);
		if (!NT_SUCCESS(status))
			break;

		*pos = ctx->ctx.iobase_reg ?
			pal_read32(ctx->ctx.iobase_reg, MOTU_REG_DMA_CTRL) : 0;
		bytesReturned = sizeof(uint32_t);
		break;
	}

	case IOCTL_MOTU424_READ_REG: {
		PMOTU424_REG_OP regOp;
		size_t bufSize;

		status = WdfRequestRetrieveInputBuffer(Request,
			sizeof(MOTU424_REG_OP), (void **)&regOp, &bufSize);
		if (!NT_SUCCESS(status))
			break;

		switch (regOp->bar) {
		case 0:
			regOp->value = pal_read32(ctx->ctx.iobase_dsp,
						   regOp->offset);
			break;
		case 1:
			regOp->value = pal_read32(ctx->ctx.iobase_reg,
						   regOp->offset);
			break;
		case 2:
			regOp->value = pal_read32(ctx->ctx.iobase_port,
						   regOp->offset);
			break;
		default:
			status = STATUS_INVALID_PARAMETER;
			break;
		}

		/* Copy result to output buffer */
		if (NT_SUCCESS(status)) {
			PMOTU424_REG_OP outOp;
			status = WdfRequestRetrieveOutputBuffer(Request,
				sizeof(MOTU424_REG_OP), (void **)&outOp,
				&bufSize);
			if (NT_SUCCESS(status)) {
				*outOp = *regOp;
				bytesReturned = sizeof(MOTU424_REG_OP);
			}
		}
		break;
	}

	case IOCTL_MOTU424_WRITE_REG: {
		PMOTU424_REG_OP regOp;
		size_t bufSize;

		status = WdfRequestRetrieveInputBuffer(Request,
			sizeof(MOTU424_REG_OP), (void **)&regOp, &bufSize);
		if (!NT_SUCCESS(status))
			break;

		switch (regOp->bar) {
		case 0:
			pal_write32(ctx->ctx.iobase_dsp, regOp->offset,
				    regOp->value);
			break;
		case 1:
			pal_write32(ctx->ctx.iobase_reg, regOp->offset,
				    regOp->value);
			break;
		case 2:
			pal_write32(ctx->ctx.iobase_port, regOp->offset,
				    regOp->value);
			break;
		default:
			status = STATUS_INVALID_PARAMETER;
			break;
		}
		break;
	}

	case IOCTL_MOTU424_GET_STATUS: {
		uint32_t *status_val;
		size_t bufSize;

		status = WdfRequestRetrieveOutputBuffer(Request,
			sizeof(uint32_t), (void **)&status_val, &bufSize);
		if (!NT_SUCCESS(status))
			break;

		*status_val = ctx->ctx.iobase_port ?
			pal_read32(ctx->ctx.iobase_port, 0) : 0;
		bytesReturned = sizeof(uint32_t);
		break;
	}

	default:
		status = STATUS_INVALID_DEVICE_REQUEST;
		break;
	}

	WdfRequestCompleteWithInformation(Request, status, bytesReturned);
	return status;
}
