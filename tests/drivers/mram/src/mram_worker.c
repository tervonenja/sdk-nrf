/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/cache.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>

#include "mram_concurrent.h"

#define TEST_PARTITION	      storage_partition
#define TEST_PARTITION_OFFSET PARTITION_OFFSET(TEST_PARTITION)
#define TEST_PARTITION_DEVICE PARTITION_DEVICE(TEST_PARTITION)

/* Each writer owns a distinct area within the partition. */
#define PPR_AREA_OFFSET	 (TEST_PARTITION_OFFSET + MRAM_PPR_AREA_OFFSET)
#define FLPR_AREA_OFFSET (TEST_PARTITION_OFFSET + MRAM_FLPR_AREA_OFFSET)
#define APP_AREA_OFFSET	 (TEST_PARTITION_OFFSET + MRAM_APP_AREA_OFFSET)

/* CPU-visible address for a partition offset (same mapping the MRAM driver uses). */
#define MRAM_ABS_ADDR(off) (DT_REG_ADDR(DT_NODELABEL(mram1x)) + (off))

static void print_block(const char *label, const uint8_t *buf, size_t len)
{
	printk("%s:", label);
	for (size_t i = 0; i < len; i++) {
		printk(" %02x", buf[i]);
	}
	printk("\n");
}

int mram_writer_run(uint8_t core_id, struct mram_test_status *status,
		    volatile struct mram_test_control *ctrl)
{
	const struct device *flash_dev = TEST_PARTITION_DEVICE;
	uint8_t pattern[MRAM_TEST_PATTERN_SIZE];
	uint32_t area_offset = (core_id == MRAM_CORE_PPR) ? PPR_AREA_OFFSET : FLPR_AREA_OFFSET;
	int64_t end;
	int err = 0;

	status->ready = 0;
	status->done = 0;
	status->iterations = 0;
	status->result = 0;
        status->longest_time = 0;
        status->errors = 0;
        status->erases = 0;

	const char *core_str = (core_id == MRAM_CORE_PPR) ? "cpuppr" : "cpuflpr";

	if (!device_is_ready(flash_dev)) {
		status->result = -ENODEV;
		status->done = MRAM_STATUS_DONE;
                printk("%s: flash device not ready\n", core_str);
		return -ENODEV;
	}

	mram_fill_pattern(core_id, pattern, sizeof(pattern));

	printk("\n\n%s start: core_id=0x%02x pattern=%u block=%u offset=0x%lx addr=0x%lx\n",
	       core_str, core_id, (unsigned int)sizeof(pattern), (unsigned int)MRAM_TEST_BLOCK_SIZE,
	       (unsigned long)area_offset, (unsigned long)MRAM_ABS_ADDR(area_offset));
	print_block("pattern", pattern, sizeof(pattern));

	status->ready = MRAM_STATUS_READY;

	/* Do not touch the area until the reader has erased it and released us. */
	while (true) {
		sys_cache_data_invd_range((void *)ctrl, sizeof(*ctrl));
		if (ctrl->start == MRAM_STATUS_GO) {
			break;
		}
		k_usleep(100);
	}

        printk("%s writer released by reader\n", core_str);

	end = k_uptime_get() + MRAM_TEST_DURATION_MS;

	uint32_t write_offset = 0;

	while (k_uptime_get() < end) {
		uint32_t write_start = k_cycle_get_32();

		/* Write the 16-byte pattern into the next slot of this core's block. */
		err = flash_write(flash_dev, area_offset + write_offset, pattern, sizeof(pattern));

		uint32_t write_time = k_cycle_get_32() - write_start;

		if (write_time > status->longest_time) {
			status->longest_time = write_time;
		}
		if (err) {
			status->errors++;
		}

		write_offset += MRAM_TEST_PATTERN_SIZE;
		if (write_offset >= MRAM_TEST_BLOCK_SIZE) {
			/* Last slot written: erase the block and count the wrap. */
			err = flash_erase(flash_dev, area_offset, MRAM_TEST_AREA_SIZE);
			if (err) {
				status->errors++;
			}
			status->erases++;
			write_offset = 0;
		}

		status->iterations++;

		/* Yield so the slower writer is not starved of MRAM access. */
		k_usleep(100);
	}

	/* Read back the final contents once writing has stopped. */
	uint8_t final_read[MRAM_TEST_PATTERN_SIZE];
	int read_err = flash_read(flash_dev, area_offset, final_read, sizeof(final_read));

	if (read_err == 0) {
		print_block("final read", final_read, sizeof(final_read));
	} else {
		printk("%s final read failed: %d\n", core_str, read_err);
	}

	status->result = err;
	status->done = MRAM_STATUS_DONE;

	return err;
}

int mram_reader_run(struct mram_test_status *status)
{
	const struct device *flash_dev = TEST_PARTITION_DEVICE;
	volatile struct mram_test_control *ppr_ctrl =
		(struct mram_test_control *)MRAM_PPR_CTRL_ADDR;
	volatile struct mram_test_control *flpr_ctrl =
		(struct mram_test_control *)MRAM_FLPR_CTRL_ADDR;
	uint8_t ppr_pattern[MRAM_TEST_PATTERN_SIZE];
	uint8_t flpr_pattern[MRAM_TEST_PATTERN_SIZE];
	uint8_t erased[MRAM_TEST_PATTERN_SIZE];
	uint8_t readback[MRAM_TEST_PATTERN_SIZE];
	int64_t end;
	int err = 0;

	status->ready = 0;
	status->done = 0;
	status->iterations = 0;
	status->result = 0;
	status->longest_time = 0;
        status->errors = 0;

	if (!device_is_ready(flash_dev)) {
                printk("cpuapp: flash device not ready\n");
		status->result = -ENODEV;
		status->done = MRAM_STATUS_DONE;
		return -ENODEV;
	}

	mram_fill_pattern(MRAM_CORE_PPR, ppr_pattern, sizeof(ppr_pattern));
	mram_fill_pattern(MRAM_CORE_FLPR, flpr_pattern, sizeof(flpr_pattern));
	memset(erased, 0xff, sizeof(erased));

	/* The writers own and auto-erase their areas, so cpuapp must NOT erase
	 * them here: cpuapp's cached erase can return before the MRAMC finishes,
	 * and the VPR's immediate write to that page then stalls the bus.
	 */

	/* Sanity-check cpuapp's own MRAM write/readback (separate area) before
	 * releasing the writers.
	 */
	uint8_t self_pattern[MRAM_TEST_PATTERN_SIZE];

	mram_fill_pattern(0xA5, self_pattern, sizeof(self_pattern));
	err = flash_write(flash_dev, APP_AREA_OFFSET, self_pattern, sizeof(self_pattern));
	if (err == 0) {
		err = flash_read(flash_dev, APP_AREA_OFFSET, readback, sizeof(readback));
	}
	if (err) {
		printk("cpuapp: self write/read failed: %d\n", err);
		status->result = err;
		status->done = MRAM_STATUS_DONE;
		return err;
	}
	if (memcmp(readback, self_pattern, sizeof(self_pattern)) != 0) {
		printk("cpuapp: self write/readback mismatch\n");
		print_block("wrote", self_pattern, sizeof(self_pattern));
		print_block("read ", readback, sizeof(readback));
		status->result = -EIO;
		status->done = MRAM_STATUS_DONE;
		return -EIO;
	}

	/* Release the writers now that the baseline is in place. */
	ppr_ctrl->start = MRAM_STATUS_GO;
	flpr_ctrl->start = MRAM_STATUS_GO;
	sys_cache_data_flush_range((void *)ppr_ctrl, sizeof(*ppr_ctrl));
	sys_cache_data_flush_range((void *)flpr_ctrl, sizeof(*flpr_ctrl));

	status->ready = MRAM_STATUS_READY;

	end = k_uptime_get() + MRAM_TEST_DURATION_MS;

	while (k_uptime_get() < end && err == 0) {
                uint32_t read_start = k_cycle_get_32();

                err = flash_read(flash_dev, APP_AREA_OFFSET, self_pattern, sizeof(self_pattern));

                uint32_t read_time = k_cycle_get_32() - read_start;

                if (read_time > status->longest_time) {
                        status->longest_time = read_time;
                }
                status->iterations++;

                if (err) {
                        status->errors++;
                }

		k_usleep(500);
	}

	status->result = err;
	status->done = MRAM_STATUS_DONE;

	return err;
}
