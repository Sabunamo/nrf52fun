#
# SRAMLink Library for nRF Connect SDK
#
# Ported from bambam for Zephyr RTOS
#

if(CONFIG_SRAMLINK)

zephyr_library_named(sramlink)

zephyr_library_sources(
    core/sramlink.c
    core/sramlink_device.c
    messages/sramlink_messages.c
    drivers/sl_radio_ncs.c
    drivers/cdtimer_zephyr.c
)

zephyr_library_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/drivers
    ${CMAKE_CURRENT_SOURCE_DIR}/messages
    ${CMAKE_CURRENT_SOURCE_DIR}/util
)

endif() # CONFIG_SRAMLINK
