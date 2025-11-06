#
# Copyright (c) 2024 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(INDOOR)

target_sources(app PRIVATE src/main.c src/font.c src/font_16x16.c src/prayerTime.c src/world_cities.c)

# Conditional display driver - parallel for nrf52dk, SPI TFT for nrf5340dk
if(BOARD MATCHES "nrf5340")
    target_sources(app PRIVATE src/ili9341_tft.c)
    message(STATUS "Using ili9341_tft.c (SPI TFT with HMI) for ${BOARD}")
else()
    target_sources(app PRIVATE src/ili9341_parallel.c)
    message(STATUS "Using ili9341_parallel.c (parallel interface) for ${BOARD}")
endif()

# Conditional GPS module selection (default to NEO-7M)
# To use NEO-6M: add -DUSE_NEO6M_GPS=1 to build command
# To use NEO-7M: add -DUSE_NEO7M_GPS=1 to build command (or use default)
if(DEFINED USE_NEO6M_GPS)
    target_sources(app PRIVATE src/gps_neo6m.c)
    target_compile_definitions(app PRIVATE USE_NEO6M_GPS)
    message(STATUS "Using NEO-6M GPS module")
elseif(DEFINED USE_NEO7M_GPS)
    target_sources(app PRIVATE src/gps_neo7m.c)
    target_compile_definitions(app PRIVATE USE_NEO7M_GPS)
    message(STATUS "Using NEO-7M GPS module")
else()
    # Default to NEO-7M
    target_sources(app PRIVATE src/gps_neo7m.c)
    target_compile_definitions(app PRIVATE USE_NEO7M_GPS)
    message(STATUS "Using NEO-7M GPS module (default)")
endif()

# Conditional speaker support - include speaker.c (will compile conditionally based on PWM availability)
if(CONFIG_PWM)
    target_sources(app PRIVATE src/speaker.c)
    message(STATUS "Speaker support enabled (PWM available)")
else()
    message(STATUS "Speaker support disabled (no PWM)")
endif()

# BME280 sensor support - include bme280_sensor.c if sensor is enabled
if(CONFIG_BME280)
    target_sources(app PRIVATE src/bme280_sensor.c)
    message(STATUS "BME280 sensor support enabled")
else()
    message(STATUS "BME280 sensor support disabled")
endif()

# PmodALS ambient light sensor support - always included for auto-brightness
target_sources(app PRIVATE src/pmodals_sensor.c)
message(STATUS "PmodALS ambient light sensor support enabled")