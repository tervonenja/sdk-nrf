/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>

#include "mram_concurrent.h"

#if defined(CONFIG_SOC_NRF9251_CPUPPR)
#define CORE_ID	    MRAM_CORE_PPR
#define STATUS_ADDR MRAM_PPR_STATUS_ADDR
#define CTRL_ADDR   MRAM_PPR_CTRL_ADDR
#elif defined(CONFIG_SOC_NRF9251_CPUFLPR)
#define CORE_ID	    MRAM_CORE_FLPR
#define STATUS_ADDR MRAM_FLPR_STATUS_ADDR
#define CTRL_ADDR   MRAM_FLPR_CTRL_ADDR
#else
#error "Unsupported remote core for the concurrent MRAM test"
#endif

static volatile struct mram_test_status *const status = (struct mram_test_status *)STATUS_ADDR;
static volatile struct mram_test_control *const ctrl = (struct mram_test_control *)CTRL_ADDR;

int main(void)
{
	(void)mram_writer_run(CORE_ID, (struct mram_test_status *)status, ctrl);

	return 0;
}
