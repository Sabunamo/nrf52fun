/**
 * @file sramlink.h
 * @brief SRAMLink Engine and Protocol Library for Zephyr
 *
 * Ported from bambam SRAMLink for nRF Connect SDK.
 *
 * SRAMLink receivers wake up for 5ms out of every 50ms. If they see
 * traffic, they'll stay awake looking for packets from their paired
 * transmitters until they hear silence for some period of time.
 */
#pragma once

#include "../drivers/sys_time_zephyr.h"
#include "../drivers/sl_radio.h"
#include "sramlink_event.h"

/** RX duty cycle timing */
#define RX_ON_INTERVAL          SYS_MS(5)                       /**< on for 5ms */
#define RX_OFF_INTERVAL         (SYS_MS(50) - (RX_ON_INTERVAL)) /**< out of every 50ms */
#define RX_LONG_ON_INTERVAL     SYS_MS(50)                      /**< stay on when activity seen */

#define RADIO_LOST_MAX_TIME_MS  (100) /**< max time we'll track for lost radio */

/** RSSI threshold for sticky feature (dBm) */
#define DEFAULT_RSSI_THRESHOLD  (-58)
#define RSSI_HOLD_TIME          SYS_MS(500)  /**< minimum time to stay on when RSSI high */

#define QUIET_TIME_BEFORE_TX    SYS_MS(5)    /**< quiet time needed before TX */
#define TIME_PER_TRANSMITTER    SYS_MS(3)    /**< time allowed per transmitter */
#define MAX_TRANSMIT_TIME       SYS_MS(500)  /**< max transmit duration */

/** TX priority values */
typedef enum {
    SL_PRIORITY_INVALID = 0,
    SL_PRIORITY_MIN     = 1,
    SL_PRIORITY_MAX     = 255,
} sl_priority_t;

/**
 * @brief AES decrypt buffer using radio
 * @param key 16-byte key
 * @param buffer Buffer to decrypt (in place)
 * @param buffer_length Length (must be multiple of 16)
 */
void sramlink_aes_decrypt(uint8_t *key, uint8_t *buffer, uint8_t buffer_length);

/**
 * @brief AES encrypt buffer using radio
 * @param key 16-byte key
 * @param buffer Buffer to encrypt (in place)
 * @param buffer_length Length (must be multiple of 16)
 */
void sramlink_aes_encrypt(uint8_t *key, uint8_t *buffer, uint8_t buffer_length);

/**
 * @brief Enter test mode to ignore packets and keep radio idle
 */
void sramlink_enable_force_idle(void);

/**
 * @brief Check if transmitter is busy
 * @return true if still transmitting
 */
bool sramlink_transmitter_busy(void);

/**
 * @brief Get priority of current transmission
 * @return SL_PRIORITY_INVALID if not busy, otherwise current priority
 */
sl_priority_t sramlink_current_tx_priority(void);

/**
 * @brief Check if SRAMLink is busy (RX or TX)
 * @return true if busy
 */
bool sramlink_busy(void);

/**
 * @brief Get packet count for current transmission
 * @return Number of packets transmitted
 */
uint8_t sramlink_get_packet_count(void);

/**
 * @brief Receive a frame if available
 * @param frame Destination for received frame
 * @param lqi Destination for LQI value (optional)
 * @return true if frame was received
 */
bool sramlink_receive_frame(sl_radio_frame_t *frame, uint8_t *lqi);

/**
 * @brief Transmit a frame
 * @param frame Frame to transmit (NULL to reset time only)
 * @param single_transmitter_time TX duration assuming single transmitter
 */
void sramlink_transmit_frame(sl_radio_frame_t *frame, sys_time_t single_transmitter_time);

/**
 * @brief Transmit a frame with callback
 * @param frame Frame to transmit
 * @param single_transmitter_time TX duration
 * @param new_evt_cb Callback for TX events
 */
void sramlink_transmit_frame_with_cb(
    sl_radio_frame_t *frame,
    sys_time_t single_transmitter_time,
    sl_evt_cb_t new_evt_cb);

/**
 * @brief Transmit a frame with priority
 * @param frame Frame to transmit
 * @param single_transmitter_time TX duration
 * @param priority TX priority level
 * @param new_evt_cb Callback for TX events
 * @param data Context data for callback
 * @return true if transmission started, false if rejected due to priority
 */
bool sramlink_transmit_frame_with_priority(
    sl_radio_frame_t *frame,
    sys_time_t single_transmitter_time,
    sl_priority_t priority,
    sl_evt_cb_t new_evt_cb, void *data);

/**
 * @brief Block until transmission completes
 */
void sramlink_wait_transmit_complete(void);

/**
 * @brief Enable/disable continuous RX mode
 * @param enable true for continuous RX
 */
void sramlink_continuous_rx_enable(bool enable);

/**
 * @brief Check if continuous RX is enabled
 * @return true if enabled
 */
bool sramlink_continuous_rx_is_enabled(void);

/**
 * @brief Enable/disable RX
 * @param enable true to enable
 */
void sramlink_rx_enable(bool enable);

/**
 * @brief Check if RX is enabled
 * @return true if enabled
 */
bool sramlink_rx_is_enabled(void);

/**
 * @brief Set RX sensitivity
 * @param sensitivity Sensitivity level
 */
void sramlink_set_rx_sensitivity(sl_radio_rx_sensitivity_t sensitivity);

/**
 * @brief Set TX power
 * @param power Power level
 */
void sramlink_set_tx_power(sl_radio_tx_power_t power);

/**
 * @brief Set Start of Frame Delimiter
 * @param sfd SFD value
 */
void sramlink_set_sfd(uint8_t sfd);

/**
 * @brief Set RSSI threshold for sticky feature
 * @param new_rssi_threshold Threshold in dBm (127 to disable)
 */
void sramlink_set_rssi_threshold(int8_t new_rssi_threshold);

/**
 * @brief Check if channel is valid
 * @param channel_number Channel to validate
 * @return true if valid
 */
bool sramlink_valid_channel(uint8_t channel_number);

/**
 * @brief Set radio channel
 * @param channel_number Channel (11-26)
 */
void sramlink_set_channel(uint8_t channel_number);

/**
 * @brief Get current radio channel
 * @return Channel number
 */
uint8_t sramlink_get_channel(void);

/**
 * @brief Set data rate
 * @param data_rate Data rate value
 */
void sramlink_set_data_rate(uint8_t data_rate);

/**
 * @brief Check if radio is awake
 * @return true if awake
 */
bool sramlink_radio_is_awake(void);

/**
 * @brief Update state machine - call in main loop
 */
void sramlink_update(void);

/**
 * @brief Stop SRAMLink engine
 */
void sramlink_stop(void);

/**
 * @brief Start SRAMLink engine
 */
void sramlink_start(void);

/**
 * @brief Notify that radio was stolen (for shared radio)
 */
void sramlink_radio_lost(void);

/**
 * @brief Notify that radio was acquired (for shared radio)
 */
void sramlink_radio_acquired(void);

/** Reset cumulative packet count */
void sramlink_reset_cumulative_packet_count(void);

/** Get cumulative packet count */
uint32_t sramlink_get_cumulative_packet_count(void);

/** Get count of other transmitters heard */
uint8_t sramlink_get_other_count(void);

/** Get radio wake time in sys_time units */
uint32_t sramlink_get_radio_wake_time(void);

/** Reset radio wake time */
void sramlink_reset_radio_wake_time(void);

/** Get max other transmitter count */
uint8_t sramlink_get_max_other_count(void);

/** Reset max other transmitter count */
void sramlink_reset_max_other_count(void);

/** Get total added jitter */
uint32_t sramlink_get_added_jitter(void);

/** Reset added jitter counter */
void sramlink_reset_added_jitter(void);

/** Check if ED sticky feature is active */
bool sramlink_is_ed_latched(void);
