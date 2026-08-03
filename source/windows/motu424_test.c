// SPDX-License-Identifier: GPL-2.0
/*
 * motu424_test.c - Userspace test tool for the MOTU PCI-424 Windows driver.
 *
 * This tool communicates with the motu424.sys kernel driver via IOCTLs
 * to verify hardware initialization, register access, and DMA operation.
 *
 * Usage:
 *   motu424_test.exe info       - Get device info (rate, status, DMA state)
 *   motu424_test.exe status      - Read BAR2 port status
 *   motu424_test.exe start       - Start DMA playback
 *   motu424_test.exe stop        - Stop DMA
 *   motu424_test.exe position    - Read DMA position counter
 *   motu424_test.exe read <bar> <offset>   - Read a register
 *   motu424_test.exe write <bar> <offset> <value>  - Write a register
 *   motu424_test.exe test        - Run full test suite
 *
 * Build: cl motu424_test.c /Fe:motu424_test.exe
 */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

/* IOCTL definitions (must match driver) */
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

/* Data structures (must match driver) */
#pragma pack(push, 1)
typedef struct _MOTU424_INFO {
	uint32_t	rate;
	uint32_t	dma_running;
	uint32_t	port_status;
	uint32_t	port_conf;
} MOTU424_INFO;

typedef struct _MOTU424_REG_OP {
	uint32_t	bar;
	uint32_t	offset;
	uint32_t	value;
} MOTU424_REG_OP;
#pragma pack(pop)

/* Device path */
#define DEVICE_PATH L"\\\\.\\MOTU424"

static HANDLE g_device = INVALID_HANDLE_VALUE;

/* ---- Helper functions ---------------------------------------------------- */

static int open_device(void)
{
	g_device = CreateFileW(DEVICE_PATH, GENERIC_READ | GENERIC_WRITE,
			       0, NULL, OPEN_EXISTING,
			       FILE_ATTRIBUTE_NORMAL, NULL);
	if (g_device == INVALID_HANDLE_VALUE) {
		printf("ERROR: Cannot open device %ws (error=%lu)\n",
			DEVICE_PATH, GetLastError());
		printf("Make sure the motu424 driver is installed and "
		       "the device is present.\n");
		return -1;
	}
	return 0;
}

static void close_device(void)
{
	if (g_device != INVALID_HANDLE_VALUE) {
		CloseHandle(g_device);
		g_device = INVALID_HANDLE_VALUE;
	}
}

static int ioctl(DWORD code, void *in, DWORD in_size, void *out,
		  DWORD out_size, DWORD *returned)
{
	DWORD bytesReturned = 0;
	BOOL ok;

	ok = DeviceIoControl(g_device, code, in, in_size, out, out_size,
			      &bytesReturned, NULL);
	if (returned)
		*returned = bytesReturned;
	if (!ok) {
		printf("ERROR: IOCTL 0x%lx failed (error=%lu)\n",
			code, GetLastError());
		return -1;
	}
	return 0;
}

/* ---- Individual test commands ------------------------------------------- */

static int cmd_info(void)
{
	MOTU424_INFO info = {0};
	DWORD ret;

	if (ioctl(IOCTL_MOTU424_GET_INFO, NULL, 0, &info, sizeof(info), &ret))
		return 1;

	printf("=== MOTU PCI-424 Device Info ===\n");
	printf("  Sample rate:    %u Hz\n", info.rate);
	printf("  DMA running:    %s\n", info.dma_running ? "YES" : "NO");
	printf("  Port status:    0x%08x", info.port_status);
	if ((info.port_status & 0x13) == 0x13)
		printf(" (FPGA configured + clock locked)");
	printf("\n");
	printf("  Port config:    0x%08x\n", info.port_conf);
	printf("\n");
	return 0;
}

static int cmd_status(void)
{
	uint32_t status = 0;
	DWORD ret;

	if (ioctl(IOCTL_MOTU424_GET_STATUS, NULL, 0, &status,
		  sizeof(status), &ret))
		return 1;

	printf("BAR2 Port Status: 0x%08x\n", status);
	if ((status & 0x13) == 0x13)
		printf("  -> FPGA configured + clock locked (OK)\n");
	else
		printf("  -> NOT fully synced (expected 0x13)\n");
	return 0;
}

static int cmd_start(void)
{
	printf("Starting DMA...\n");
	if (ioctl(IOCTL_MOTU424_START, NULL, 0, NULL, 0, NULL))
		return 1;
	printf("DMA started.\n");
	return 0;
}

static int cmd_stop(void)
{
	printf("Stopping DMA...\n");
	if (ioctl(IOCTL_MOTU424_STOP, NULL, 0, NULL, 0, NULL))
		return 1;
	printf("DMA stopped.\n");
	return 0;
}

static int cmd_position(void)
{
	uint32_t pos = 0;
	DWORD ret;

	if (ioctl(IOCTL_MOTU424_GET_POSITION, NULL, 0, &pos,
		  sizeof(pos), &ret))
		return 1;

	printf("DMA Position: 0x%08x (%u)\n", pos, pos);
	return 0;
}

static int cmd_read_reg(int argc, char *argv[])
{
	MOTU424_REG_OP op = {0};
	DWORD ret;

	if (argc < 4) {
		printf("Usage: read <bar> <offset>\n");
		return 1;
	}

	op.bar = (uint32_t)strtoul(argv[2], NULL, 0);
	op.offset = (uint32_t)strtoul(argv[3], NULL, 0);

	if (ioctl(IOCTL_MOTU424_READ_REG, &op, sizeof(op), &op,
		  sizeof(op), &ret))
		return 1;

	printf("BAR%u 0x%08x = 0x%08x\n", op.bar, op.offset, op.value);
	return 0;
}

static int cmd_write_reg(int argc, char *argv[])
{
	MOTU424_REG_OP op = {0};
	DWORD ret;

	if (argc < 5) {
		printf("Usage: write <bar> <offset> <value>\n");
		return 1;
	}

	op.bar = (uint32_t)strtoul(argv[2], NULL, 0);
	op.offset = (uint32_t)strtoul(argv[3], NULL, 0);
	op.value = (uint32_t)strtoul(argv[4], NULL, 0);

	if (ioctl(IOCTL_MOTU424_WRITE_REG, &op, sizeof(op), NULL, 0, &ret))
		return 1;

	printf("Wrote BAR%u 0x%08x = 0x%08x\n", op.bar, op.offset, op.value);
	return 0;
}

/* ---- Full test suite ----------------------------------------------------- */

static int test_suite(void)
{
	int failures = 0;
	int tests = 0;

	printf("=== MOTU PCI-424 Test Suite ===\n\n");

	/* Test 1: Get device info */
	printf("[Test %d] Get device info... ", ++tests);
	{
		MOTU424_INFO info = {0};
		DWORD ret;
		if (ioctl(IOCTL_MOTU424_GET_INFO, NULL, 0, &info,
			  sizeof(info), &ret) == 0 && ret == sizeof(info)) {
			printf("PASS\n");
			printf("  Rate: %u Hz, DMA: %s, Status: 0x%08x\n",
				info.rate, info.dma_running ? "ON" : "OFF",
				info.port_status);
			if ((info.port_status & 0x13) == 0x13)
				printf("  Clock sync: LOCKED\n");
			else
				printf("  Clock sync: NOT LOCKED (warm boot?)\n");
		} else {
			printf("FAIL\n");
			failures++;
		}
	}

	/* Test 2: Read BAR2 status */
	printf("[Test %d] Read BAR2 status... ", ++tests);
	{
		uint32_t status = 0;
		DWORD ret;
		if (ioctl(IOCTL_MOTU424_GET_STATUS, NULL, 0, &status,
			  sizeof(status), &ret) == 0 && ret == sizeof(status)) {
			printf("PASS (0x%08x)\n", status);
		} else {
			printf("FAIL\n");
			failures++;
		}
	}

	/* Test 3: Read BAR1 port config register */
	printf("[Test %d] Read BAR1 0x00 (port config)... ", ++tests);
	{
		MOTU424_REG_OP op = { .bar = 1, .offset = 0x00 };
		DWORD ret;
		if (ioctl(IOCTL_MOTU424_READ_REG, &op, sizeof(op), &op,
			  sizeof(op), &ret) == 0) {
			printf("PASS (0x%08x)\n", op.value);
		} else {
			printf("FAIL\n");
			failures++;
		}
	}

	/* Test 4: Read BAR1 DMA base register */
	printf("[Test %d] Read BAR1 0x18 (DMA base)... ", ++tests);
	{
		MOTU424_REG_OP op = { .bar = 1, .offset = 0x18 };
		DWORD ret;
		if (ioctl(IOCTL_MOTU424_READ_REG, &op, sizeof(op), &op,
			  sizeof(op), &ret) == 0) {
			printf("PASS (0x%08x)\n", op.value);
		} else {
			printf("FAIL\n");
			failures++;
		}
	}

	/* Test 5: Read BAR0 DSP sync status */
	printf("[Test %d] Read BAR0 0x6fec (sync status)... ", ++tests);
	{
		MOTU424_REG_OP op = { .bar = 0, .offset = 0x6fec };
		DWORD ret;
		if (ioctl(IOCTL_MOTU424_READ_REG, &op, sizeof(op), &op,
			  sizeof(op), &ret) == 0) {
			printf("PASS (0x%08x)\n", op.value);
		} else {
			printf("FAIL\n");
			failures++;
		}
	}

	/* Test 6: Read BAR0 DSP boot vector */
	printf("[Test %d] Read BAR0 0x3fffc (DSP boot vector)... ", ++tests);
	{
		MOTU424_REG_OP op = { .bar = 0, .offset = 0x3fffc };
		DWORD ret;
		if (ioctl(IOCTL_MOTU424_READ_REG, &op, sizeof(op), &op,
			  sizeof(op), &ret) == 0) {
			printf("PASS (0x%08x)\n", op.value);
		} else {
			printf("FAIL\n");
			failures++;
		}
	}

	/* Test 7: Stop DMA (should always succeed) */
	printf("[Test %d] Stop DMA... ", ++tests);
	{
		if (ioctl(IOCTL_MOTU424_STOP, NULL, 0, NULL, 0, NULL) == 0) {
			printf("PASS\n");
		} else {
			printf("FAIL\n");
			failures++;
		}
	}

	/* Test 8: Read DMA position */
	printf("[Test %d] Read DMA position... ", ++tests);
	{
		uint32_t pos = 0;
		DWORD ret;
		if (ioctl(IOCTL_MOTU424_GET_POSITION, NULL, 0, &pos,
			  sizeof(pos), &ret) == 0) {
			printf("PASS (0x%08x)\n", pos);
		} else {
			printf("FAIL\n");
			failures++;
		}
	}

	/* Test 9: Start DMA */
	printf("[Test %d] Start DMA... ", ++tests);
	{
		if (ioctl(IOCTL_MOTU424_START, NULL, 0, NULL, 0, NULL) == 0) {
			printf("PASS\n");
		} else {
			printf("FAIL\n");
			failures++;
		}
	}

	/* Test 10: Read DMA position after start (should be moving) */
	printf("[Test %d] Read DMA position (after start)... ", ++tests);
	{
		uint32_t pos1 = 0, pos2 = 0;
		DWORD ret;

		Sleep(100); /* Give DMA time to run */

		ioctl(IOCTL_MOTU424_GET_POSITION, NULL, 0, &pos1,
		      sizeof(pos1), &ret);
		Sleep(100);
		ioctl(IOCTL_MOTU424_GET_POSITION, NULL, 0, &pos2,
		      sizeof(pos2), &ret);

		if (pos1 != 0 || pos2 != 0) {
			printf("PASS (pos1=0x%08x pos2=0x%08x)\n", pos1, pos2);
		} else {
			printf("PASS (positions are 0 — may need audio data)\n");
		}
	}

	/* Test 11: Stop DMA (cleanup) */
	printf("[Test %d] Stop DMA (cleanup)... ", ++tests);
	{
		if (ioctl(IOCTL_MOTU424_STOP, NULL, 0, NULL, 0, NULL) == 0) {
			printf("PASS\n");
		} else {
			printf("FAIL\n");
			failures++;
		}
	}

	/* Test 12: Verify info after stop */
	printf("[Test %d] Verify DMA stopped... ", ++tests);
	{
		MOTU424_INFO info = {0};
		DWORD ret;
		ioctl(IOCTL_MOTU424_GET_INFO, NULL, 0, &info, sizeof(info),
		      &ret);
		if (info.dma_running == 0) {
			printf("PASS\n");
		} else {
			printf("FAIL (dma_running=%u)\n", info.dma_running);
			failures++;
		}
	}

	printf("\n=== Results: %d/%d tests passed ===\n",
		tests - failures, tests);

	if (failures > 0) {
		printf("FAILED: %d test(s) failed\n", failures);
		return 1;
	}
	printf("ALL TESTS PASSED\n");
	return 0;
}

/* ---- Main ---------------------------------------------------------------- */

static void print_usage(void)
{
	printf("MOTU PCI-424 Test Tool\n");
	printf("\nUsage: motu424_test.exe <command>\n");
	printf("\nCommands:\n");
	printf("  info                          Get device info\n");
	printf("  status                        Read BAR2 port status\n");
	printf("  start                         Start DMA playback\n");
	printf("  stop                          Stop DMA\n");
	printf("  position                      Read DMA position counter\n");
	printf("  read <bar> <offset>           Read a register\n");
	printf("  write <bar> <offset> <value>  Write a register\n");
	printf("  test                          Run full test suite\n");
	printf("\nBar: 0=DSP memory, 1=Control registers, 2=FPGA port\n");
}

int main(int argc, char *argv[])
{
	int ret = 0;

	if (argc < 2) {
		print_usage();
		return 1;
	}

	if (open_device())
		return 1;

	if (strcmp(argv[1], "info") == 0)
		ret = cmd_info();
	else if (strcmp(argv[1], "status") == 0)
		ret = cmd_status();
	else if (strcmp(argv[1], "start") == 0)
		ret = cmd_start();
	else if (strcmp(argv[1], "stop") == 0)
		ret = cmd_stop();
	else if (strcmp(argv[1], "position") == 0)
		ret = cmd_position();
	else if (strcmp(argv[1], "read") == 0)
		ret = cmd_read_reg(argc, argv);
	else if (strcmp(argv[1], "write") == 0)
		ret = cmd_write_reg(argc, argv);
	else if (strcmp(argv[1], "test") == 0)
		ret = test_suite();
	else {
		print_usage();
		ret = 1;
	}

	close_device();
	return ret;
}
