#
# Copyright (c) 2024 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(l1_e1_1)

target_sources(app PRIVATE src/main.c src/font.c src/prayerTime.c src/gps.c src/ili9341_tft.c src/world_cities.c src/speaker.c)