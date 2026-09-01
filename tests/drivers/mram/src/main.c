/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/cache.h>
#include <zephyr/devicetree.h>

#if defined(CONFIG_MRAM_LATENCY)
#include <soc/nordic/common/mram_latency.h>
#endif

#include "mram_concurrent.h"

/* Each VPR core reports through a block in its own TCM that cpuapp can read. */
static volatile struct mram_test_status *const ppr_status =
	(struct mram_test_status *)MRAM_PPR_STATUS_ADDR;
static volatile struct mram_test_status *const flpr_status =
	(struct mram_test_status *)MRAM_FLPR_STATUS_ADDR;

static bool remote_passed(volatile struct mram_test_status *st, const char *name, int64_t deadline)
{
	while (k_uptime_get() < deadline) {
		sys_cache_data_invd_range((void *)st, sizeof(*st));
		if (st->ready == MRAM_STATUS_READY && st->done == MRAM_STATUS_DONE) {
			printk("%s: result=%d, longest=%u cycles, errors=%u, iterations=%u\n", name, st->result, st->longest_time, st->errors, st->iterations);
			return (st->result == 0) && (st->iterations > 0);
		}
		k_msleep(50);
	}

	printk("%s: did not finish in time\n", name);
	return false;
}

/* Set define based on if mutex node is found from mram1x_controller devicetree node */
#define USING_MRAM_MUTEX DT_NODE_HAS_PROP(DT_NODELABEL(mram1x_controller), nordic_mutex)

int main(void)
{
	struct mram_test_status app_status;
	bool pass = true;

	//printk("Concurrent MRAM access test on %s\n", CONFIG_BOARD_TARGET);

        printk("\nMRAM latency mode: %s\n", IS_ENABLED(CONFIG_MRAM_LATENCY) ? "enabled" : "disabled");
        printk("MRAMC ready bit check: %s\n", IS_ENABLED(CONFIG_NRF_MRAM_CHECK_READY_BIT) ? "enabled" : "disabled");
        printk("MRAM mutex: %s\n", USING_MRAM_MUTEX ? "enabled" : "disabled");
        printk("MRAM write size = %u, erase size = %u\n", (unsigned int)MRAM_TEST_PATTERN_SIZE, (unsigned int)MRAM_TEST_BLOCK_SIZE);

#if defined(CONFIG_MRAM_LATENCY)
        int rc = mram_no_latency_sync_request();
        if (rc) {
                printk("Failed to request no-latency mode for MRAM write");
                return -EIO;
        }
#endif

	/* cpuapp only reads; cpuppr and cpuflpr write their own patterns. */
	(void)mram_reader_run(&app_status);

	printk("cpuapp: result=%d, longest=%u cycles, errors=%u, iterations=%u\n", app_status.result, app_status.longest_time, app_status.errors, app_status.iterations);
	if (app_status.result != 0 || app_status.iterations == 0) {
		pass = false;
	}

	/* Collect the writer results. */
	int64_t deadline = k_uptime_get() + 5000;

	if (!remote_passed(ppr_status, "cpuppr", deadline)) {
		pass = false;
	}
	if (!remote_passed(flpr_status, "cpuflpr", deadline)) {
		pass = false;
	}

	printk("Tests status: %s\n", pass ? "PASS" : "FAIL");

	return 0;
}
