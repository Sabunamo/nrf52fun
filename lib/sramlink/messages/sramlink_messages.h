/**
 * @file sramlink_messages.h
 * @brief SRAMLink Message Packing/Unpacking for Zephyr
 *
 * Simplified port from bambam - focuses on essential message types.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../drivers/sl_radio.h"
#include "../core/sramlink_device.h"
#include "sramlink_messages_common.h"

/** SRAMLink version definitions */
#define SL_VERSION_1_2      0x00
#define SL_VERSION_2_0      0x01
#define SL_VERSION_1_1      0x02
#define SL_VERSION_DEFAULT  0xEE
#define SL_VERSION_INVALID  0xFF

/** AES block size */
#define AES_BLOCK_SIZE_BYTES    16

/** Maximum payload in test message */
#define SL_TEST_PAYLOAD_MAX     (SL_RADIO_FRAME_MAX_LEN - 11 - 5)

/** Basic message structures */

/** Button message (most common) */
typedef struct {
    uint32_t rolling_code;
    uint8_t button_mask;
    uint8_t flags;
} slmsg_v2_0_buttons_t;

/** Status request */
typedef struct {
    uint32_t rolling_code;
    uint8_t request_type;
} slmsg_v2_0_status_request_t;

/** Status report */
typedef struct {
    uint32_t rolling_code;
    uint8_t index_a;
    uint8_t index_b;
    uint8_t index_c;
    uint8_t flags;
    uint16_t battery_voltage;
    uint8_t battery_status;
} slmsg_v2_0_status_report_t;

/** Device info */
typedef struct {
    uint32_t rolling_code;
    uint32_t app_version;
    uint16_t model_id;
    uint8_t protocol_bridge_version;
} slmsg_v2_0_device_info_t;

/** Test message */
typedef struct {
    uint32_t rolling_code;
    uint8_t payload_len;
    uint8_t payload[SL_TEST_PAYLOAD_MAX];
} slmsg_v2_0_test_t;

/** Network settings (for pairing) */
typedef struct {
    uint32_t rolling_code;
    uint32_t network_id;
    uint8_t network_channel;
    uint8_t network_key[AES_BLOCK_SIZE_BYTES];
} slmsg_v2_0_network_settings_t;

/** Public key share (for pairing) */
typedef struct {
    uint8_t adv_channel;
    uint8_t public_key[AES_BLOCK_SIZE_BYTES];
    uint8_t aes_challenge_response[AES_BLOCK_SIZE_BYTES];
} slmsg_v2_0_public_key_share_t;

/** Union of all v2.0 message types */
typedef union {
    slmsg_v2_0_buttons_t buttons;
    slmsg_v2_0_status_request_t status_request;
    slmsg_v2_0_status_report_t status_report;
    slmsg_v2_0_device_info_t device_info;
    slmsg_v2_0_test_t test;
    slmsg_v2_0_network_settings_t network_settings;
    slmsg_v2_0_public_key_share_t public_key_share;
    uint32_t rolling_code;  /* Quick access to rolling code */
} slmsg_v2_0_union_t;

/** Union of all message types */
typedef union {
    slmsg_v2_0_union_t v2_0;
    uint32_t rolling_code;
} slmsg_union_t;

/**
 * @brief Get header length for current default version
 * @return Header length in bytes
 */
uint8_t slmsg_get_header_length(void);

/**
 * @brief Pack header to radio frame
 * @param header Header to pack
 * @param frame Destination frame
 */
void slmsg_pack_header(slmsg_header_t *header, sl_radio_frame_t *frame);

/**
 * @brief Unpack header from radio frame
 * @param frame Source frame
 * @param header Destination header
 */
void slmsg_unpack_header(sl_radio_frame_t *frame, slmsg_header_t *header);

/**
 * @brief Pack a message into radio frame using specified version
 * @param version Protocol version to use
 * @param header Message header
 * @param data_struct Message payload structure
 * @param key Encryption key (NULL for unencrypted)
 * @param frame Destination radio frame
 * @return true if packed successfully
 */
bool slmsg_pack_msg_by_version(uint8_t version, slmsg_header_t *header,
                               void *data_struct, uint8_t *key,
                               sl_radio_frame_t *frame);

/**
 * @brief Pack a message using default version
 * @param header Message header
 * @param data_struct Message payload structure
 * @param key Encryption key (NULL for unencrypted)
 * @param frame Destination radio frame
 * @return true if packed successfully
 */
bool slmsg_pack_msg(slmsg_header_t *header, void *data_struct,
                    uint8_t *key, sl_radio_frame_t *frame);

/**
 * @brief Unpack a message from radio frame
 * @param frame Source radio frame
 * @param key Decryption key (NULL for unencrypted)
 * @param header Destination header
 * @param data_struct Destination payload structure
 * @return true if unpacked successfully
 */
bool slmsg_unpack_msg(sl_radio_frame_t *frame, uint8_t *key,
                      slmsg_header_t *header, void *data_struct);

/**
 * @brief Set the default protocol version
 * @param version Version to use (SL_VERSION_*)
 */
void slmsg_set_default_version(uint8_t version);

/**
 * @brief Get the current default version
 * @return Current default version
 */
uint8_t slmsg_get_default_version(void);

/**
 * @brief Check if version is valid
 * @param version Version to check
 * @return true if valid
 */
bool slmsg_is_valid_version(uint8_t version);

/**
 * @brief Initialize header for broadcast message
 * @param sldev Device structure
 * @param msg_type Message type
 * @param header Destination header
 */
void slmsg_init_broadcast_header(sramlink_device_t *sldev, uint8_t msg_type,
                                 slmsg_header_t *header);

/**
 * @brief Initialize header for targeted message
 * @param sldev Device structure
 * @param target_did Target device ID
 * @param msg_type Message type
 * @param header Destination header
 */
void slmsg_init_targeted_header(sramlink_device_t *sldev, uint32_t target_did,
                                uint8_t msg_type, slmsg_header_t *header);

/**
 * @brief Initialize header for global broadcast
 * @param sldev Device structure
 * @param msg_type Message type
 * @param header Destination header
 */
void slmsg_init_global_broadcast_header(sramlink_device_t *sldev,
                                        uint8_t msg_type,
                                        slmsg_header_t *header);

/**
 * @brief Check if message is targeted to this device
 * @param header Message header
 * @param sldev Device structure
 * @return true if targeted to us
 */
bool slmsg_is_targeted_to_me(slmsg_header_t *header, sramlink_device_t *sldev);

/**
 * @brief Get pointer to current header being unpacked
 * @return Header pointer or NULL
 */
slmsg_header_t *slmsg_get_current_header(void);

/**
 * @brief Unpack rolling code from frame
 * @param frame Source frame
 * @param rolling_code Destination
 * @return true if has rolling code
 */
bool slmsg_unpack_rolling_code(sl_radio_frame_t *frame, uint32_t *rolling_code);

/**
 * @brief Check if message type has rolling code
 * @param version Protocol version
 * @param msg_type Message type
 * @return true if has rolling code
 */
bool slmsg_has_rolling_code(uint8_t version, slmsg_msg_type_t msg_type);

/**
 * @brief Check if message type has payload
 * @param version Protocol version
 * @param msg_type Message type
 * @return true if has payload
 */
bool slmsg_has_payload(uint8_t version, slmsg_msg_type_t msg_type);
