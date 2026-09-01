/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef MRAM_CONCURRENT_H_
#define MRAM_CONCURRENT_H_

#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>

/* Duration the writers keep writing and the reader keeps reading. */
#define MRAM_TEST_DURATION_MS 10000

/* MRAM write-block size and the area cpuapp erases up front. */
#define MRAM_TEST_BLOCK_SIZE 4096
#define MRAM_TEST_PATTERN_SIZE 32
#define MRAM_TEST_AREA_SIZE MRAM_TEST_BLOCK_SIZE

/* Per-writer identity mixed into the pattern each writer writes. */
#define MRAM_CORE_PPR  0xB2
#define MRAM_CORE_FLPR 0xC3

/* Each writer owns a distinct area within the storage partition. */
#define MRAM_PPR_AREA_OFFSET  0
#define MRAM_FLPR_AREA_OFFSET MRAM_TEST_BLOCK_SIZE

/* cpuapp uses its own area for the pre-test self write/readback check. */
#define MRAM_APP_AREA_OFFSET (2 * MRAM_TEST_BLOCK_SIZE)

/* Hand-shake magics placed in the shared memory blocks. */
#define MRAM_STATUS_READY 0x52454459 /* "REDY" */
#define MRAM_STATUS_DONE  0x444F4E45 /* "DONE" */
#define MRAM_STATUS_GO	  0x2147474F /* reader released the writers */

/* Writer -> reader (cpuapp) status blocks, in each VPR's TCM. */
#define MRAM_PPR_STATUS_ADDR  DT_REG_ADDR(DT_NODELABEL(cpuppr_cpuapp_ipc_shm))
#define MRAM_FLPR_STATUS_ADDR DT_REG_ADDR(DT_NODELABEL(cpuflpr_cpuapp_ipc_shm))

/* Reader (cpuapp) -> writer control blocks, in each VPR's TCM. */
#define MRAM_PPR_CTRL_ADDR  DT_REG_ADDR(DT_NODELABEL(cpuapp_cpuppr_ipc_shm))
#define MRAM_FLPR_CTRL_ADDR DT_REG_ADDR(DT_NODELABEL(cpuapp_cpuflpr_ipc_shm))

/* Reported by each core through a shared memory block. */
struct mram_test_status {
	volatile uint32_t ready;
	volatile uint32_t done;
	volatile uint32_t iterations;
	volatile int32_t result; /* 0 = pass, negative errno = failure */
	volatile uint32_t longest_time;
	volatile uint32_t errors;
	volatile uint32_t erases; /* writer: times its block was filled and erased */
};

/* cpuapp -> writer release flag. */
struct mram_test_control {
	volatile uint32_t start;
};

static inline void mram_fill_pattern(uint8_t core_id, uint8_t *buf, size_t len)
{
	for (size_t i = 0; i < len; i++) {
		buf[i] = (uint8_t)(core_id ^ (uint8_t)i);
	}
}

/*
 * Writer role (cpuppr / cpuflpr): wait until the reader has erased the area and
 * set @p ctrl->start, then repeatedly write this core's pattern for
 * MRAM_TEST_DURATION_MS. Writes are serialized across cores by the MRAM
 * driver's own hardware mutex.
 *
 * @return 0 on success, negative errno on the first failure.
 */
int mram_writer_run(uint8_t core_id, struct mram_test_status *status,
		    volatile struct mram_test_control *ctrl);

/*
 * Reader role (cpuapp): erase both writer areas, release the writers, then
 * repeatedly read each area for MRAM_TEST_DURATION_MS and verify it holds that
 * writer's pattern (or the erased baseline before the writer has run).
 *
 * @return 0 on success, negative errno on failure.
 */
int mram_reader_run(struct mram_test_status *status);

#endif /* MRAM_CONCURRENT_H_ */
