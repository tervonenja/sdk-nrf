#
# Copyright (c) 2026 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(mram_concurrent)

zephyr_include_directories(include ${ZEPHYR_BASE})

target_sources(app PRIVATE
  src/main.c
  src/mram_worker.c
)
