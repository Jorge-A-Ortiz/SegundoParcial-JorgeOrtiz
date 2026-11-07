
# Copyright 2026 NXP
#
# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.22.0)

cmake_path(APPEND conf_file_path ${CMAKE_CURRENT_LIST_DIR} ${board} prj.conf)
set(CONF_FILE ${conf_file_path} ${CONF_FILE})
option(P10_TIMING "Enable P10 UART timing" OFF)
option(P02_CAPTURE "Enable on-board P02 edge capture" OFF)
if(P02_CAPTURE)
    list(APPEND CONF_FILE ${CMAKE_CURRENT_LIST_DIR}/pruebas/p02.conf)
endif()
include(${SdkRootDirPath}/cmake/extension/mcux.cmake)

project(SegundoParcial LANGUAGES C CXX ASM PROJECT_BOARD_PORT_PATH ${board_root}/${board}/demo_apps/led_blinky)

include(${SdkRootDirPath}/CMakeLists.txt)


include(${board}/board_files.cmake)

mcux_add_source(
    SOURCES led_blinky.c
            monitor.c
            monitor.h
)

mcux_add_source(
    SOURCES led_blinky/peripherals.c
            led_blinky/peripherals.h
)
if(P02_CAPTURE)
    mcux_add_configuration(CC "-DP02_CAPTURE=1")
    mcux_add_source(SOURCES pruebas/p02_capture.c)
endif()

if(P10_TIMING AND P02_CAPTURE)
    message(FATAL_ERROR "P02_CAPTURE and P10_TIMING both use CTIMER1")
endif()
if(P10_TIMING)
    mcux_add_configuration(CC "-DP10_TIMING=1")
    mcux_add_source(SOURCES pruebas/p10_timing.c)
endif()

mcux_convert_binary(BINARY ${APPLICATION_BINARY_DIR}/${MCUX_SDK_PROJECT_NAME}.bin)
mcux_add_config_mex_path( PATH ${board} )
mcux_add_include(INCLUDES led_blinky .)
