/**
 * @file sramlink_app.h
 * @brief SRAMLink Application Integration Layer
 *
 * High-level API for integrating SRAMLink with the INDOOR application.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Callback for received button messages
 * @param device_id Sender device ID
 * @param button_mask Button state bitmask
 * @param flags Additional flags
 */
typedef void (*sramlink_button_cb_t)(uint32_t device_id, uint8_t button_mask, uint8_t flags);

/**
 * @brief Callback for received status reports
 * @param device_id Sender device ID
 * @param index_a First index value
 * @param index_b Second index value
 * @param index_c Third index value
 * @param battery_mv Battery voltage in mV
 */
typedef void (*sramlink_status_cb_t)(uint32_t device_id, uint8_t index_a,
                                     uint8_t index_b, uint8_t index_c,
                                     uint16_t battery_mv);

/**
 * @brief SRAMLink application configuration
 */
typedef struct {
    uint8_t device_type;        /**< This device's type */
    uint8_t radio_channel;      /**< Radio channel (11-26) */
    int8_t tx_power_dbm;        /**< TX power in dBm */
    bool enable_rx;             /**< Enable receiving */
    bool continuous_rx;         /**< Use continuous RX (no duty cycling) */
    sramlink_button_cb_t button_cb;   /**< Button message callback */
    sramlink_status_cb_t status_cb;   /**< Status report callback */
} sramlink_app_config_t;

/**
 * @brief Initialize SRAMLink application layer
 * @param config Configuration structure
 * @return 0 on success, negative errno on failure
 */
int sramlink_app_init(const sramlink_app_config_t *config);

/**
 * @brief Process SRAMLink state machine
 *
 * Call this regularly from main loop or a dedicated thread.
 */
void sramlink_app_process(void);

/**
 * @brief Start SRAMLink radio
 * @return 0 on success
 */
int sramlink_app_start(void);

/**
 * @brief Stop SRAMLink radio
 */
void sramlink_app_stop(void);

/**
 * @brief Check if SRAMLink is running
 * @return true if running
 */
bool sramlink_app_is_running(void);

/**
 * @brief Send a button press message
 * @param button_mask Button state bitmask
 * @param flags Additional flags
 * @return 0 on success
 */
int sramlink_app_send_buttons(uint8_t button_mask, uint8_t flags);

/**
 * @brief Send a status request
 * @param request_type Type of status to request
 * @return 0 on success
 */
int sramlink_app_send_status_request(uint8_t request_type);

/**
 * @brief Send a status report
 * @param index_a First index value
 * @param index_b Second index value
 * @param index_c Third index value
 * @param battery_mv Battery voltage in mV
 * @param battery_status Battery status byte
 * @return 0 on success
 */
int sramlink_app_send_status_report(uint8_t index_a, uint8_t index_b,
                                    uint8_t index_c, uint16_t battery_mv,
                                    uint8_t battery_status);

/**
 * @brief Send a test message with payload
 * @param payload Payload data
 * @param len Payload length
 * @return 0 on success
 */
int sramlink_app_send_test(const uint8_t *payload, uint8_t len);

/**
 * @brief Set radio channel
 * @param channel Channel number (11-26)
 * @return 0 on success
 */
int sramlink_app_set_channel(uint8_t channel);

/**
 * @brief Get current radio channel
 * @return Channel number
 */
uint8_t sramlink_app_get_channel(void);

/**
 * @brief Set TX power
 * @param power_dbm Power in dBm
 * @return 0 on success
 */
int sramlink_app_set_tx_power(int8_t power_dbm);

/**
 * @brief Check if device is paired
 * @return true if paired with at least one device
 */
bool sramlink_app_is_paired(void);

/**
 * @brief Get this device's ID
 * @return Device ID
 */
uint32_t sramlink_app_get_device_id(void);

/**
 * @brief Send a PUBLIC_KEY_SHARE message for pairing key exchange
 * @return 0 on success
 */
int sramlink_app_send_public_key_share(void);

/**
 * @brief Initiate pairing mode
 * @return 0 on success
 */
int sramlink_app_start_pairing(void);

/**
 * @brief Exit pairing mode
 */
void sramlink_app_stop_pairing(void);

/**
 * @brief Clear all pairings
 * @return 0 on success
 */
int sramlink_app_clear_pairings(void);

/**
 * @brief Check if pairing just completed (clears flag on read)
 * @return true if pairing completed since last check
 */
bool sramlink_app_pairing_just_completed(void);

/**
 * @brief Get number of paired devices
 * @return Number of paired devices
 */
uint8_t sramlink_app_get_paired_count(void);
