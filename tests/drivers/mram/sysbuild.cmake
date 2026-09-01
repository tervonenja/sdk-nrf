#
# Copyright (c) 2026 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

# Build the cpuppr and cpuflpr images and let cpuapp launch them at boot.
ExternalZephyrProject_Add(
  APPLICATION remote_ppr
  SOURCE_DIR  ${APP_DIR}/remote
  BOARD ${SB_CONFIG_BOARD}/${SB_CONFIG_SOC}/cpuppr
  BOARD_REVISION ${BOARD_REVISION}
)

ExternalZephyrProject_Add(
  APPLICATION remote_flpr
  SOURCE_DIR  ${APP_DIR}/remote
  BOARD ${SB_CONFIG_BOARD}/${SB_CONFIG_SOC}/cpuflpr
  BOARD_REVISION ${BOARD_REVISION}
)

add_custom_target(
merged_hex ALL
COMMAND ${CMAKE_COMMAND} -E echo "Merging uicr.hex, periphconf.hex, and zephyr.hex into merged.hex"

COMMAND ${PYTHON_EXECUTABLE} ${ZEPHYR_BASE}/scripts/build/mergehex.py
    -o ${CMAKE_BINARY_DIR}/merged.hex
    ${CMAKE_BINARY_DIR}/uicr/zephyr/uicr.hex
    ${CMAKE_BINARY_DIR}/uicr/zephyr/periphconf.hex
    ${CMAKE_BINARY_DIR}/${DEFAULT_IMAGE}/zephyr/zephyr.hex
    ${CMAKE_BINARY_DIR}/remote_flpr/zephyr/zephyr.hex
    ${CMAKE_BINARY_DIR}/remote_ppr/zephyr/zephyr.hex
DEPENDS
    uicr
    remote_flpr
    remote_ppr
    ${DEFAULT_IMAGE}
BYPRODUCTS
    ${CMAKE_BINARY_DIR}/merged.hex
COMMENT "Merging merged.hex"
)

add_custom_target(
uicr_periphconf_merged_hex ALL
COMMAND ${CMAKE_COMMAND} -E echo "Merging uicr.hex and periphconf.hex into uicr_periphconf.hex"

COMMAND ${PYTHON_EXECUTABLE} ${ZEPHYR_BASE}/scripts/build/mergehex.py
    -o ${CMAKE_BINARY_DIR}/uicr_periphconf.hex
    ${CMAKE_BINARY_DIR}/uicr/zephyr/uicr.hex
    ${CMAKE_BINARY_DIR}/uicr/zephyr/periphconf.hex
DEPENDS
    uicr
BYPRODUCTS
    ${CMAKE_BINARY_DIR}/uicr_periphconf.hex
COMMENT "Merging uicr.hex and periphconf.hex into uicr_periphconf.hex"
)
