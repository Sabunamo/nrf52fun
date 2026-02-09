/**
 * @file sramlink_device.h
 * @brief SRAMLink Device State Management for Zephyr
 *
 * Handles device identity, pairing records, and NVS persistence.
 * Ported from bambam for nRF Connect SDK.
 */
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "../drivers/sl_radio.h"
#include "../messages/sramlink_messages_common.h"

/** Max number of devices we can be paired to */
#define MAX_PAIRING_RECORDS (16)

/** Radio channel to use when initializing with pairing seed */
#define PAIRING_SEED_RADIO_CHANNEL (26)

/** Allowed difference between rolling codes */
#define MESSAGE_DISTANCE_LIMIT 4096

/**
 * @brief Data stored for each paired device
 */
typedef struct {
    bool active;                    /**< Is this record active */
    uint8_t device_type;            /**< Device type */
    uint8_t protocol_bridge_version;/**< Protocol bridge version */
    uint8_t PADDING_0_3;            /**< Padding for alignment */
    uint32_t device_id;             /**< Device ID */
    uint32_t rolling_code;          /**< Rolling code */
    uint16_t model_id;              /**< Model ID */
    uint8_t PADDING_3_2;            /**< Padding */
    uint8_t PADDING_3_3;            /**< Padding */
    uint32_t app_version;           /**< Application firmware version */
} sramlink_paired_data_t;

/**
 * @brief Main device state structure
 */
typedef struct {
    uint8_t device_type;            /**< This device's type */
    uint32_t device_id;             /**< This device's ID */
    uint32_t rolling_code;          /**< Current rolling code */
    uint16_t model_id;              /**< Model ID */
    uint32_t master_id;             /**< Master ID from pairing */
    uint8_t master_key[16];         /**< Encryption key from pairing */
    uint8_t radio_channel;          /**< Operating channel */
    sramlink_paired_data_t paired_devices[MAX_PAIRING_RECORDS];
    uint8_t flags;                  /**< Internal flags */
} sramlink_device_t;

/** Flag bit: paired rolling codes need to be stored */
#define SLDEVICE_FLAG_BIT_PAIRED_RC_NEEDS_STORE 0

/** Flag bit: device data needs to be stored */
#define SLDEVICE_FLAG_BIT_SLDEVICE_NEEDS_STORE 1

/**
 * @brief Initialize the SRAMLink device subsystem
 */
void sramlink_device_init(void);

/**
 * @brief Load device data from NVS
 * @param sld Pointer to device structure to populate
 */
void sramlink_device_load(sramlink_device_t *sld);

/**
 * @brief Store device data to NVS
 * @param sld Pointer to device structure to store
 */
void sramlink_device_store(sramlink_device_t *sld);

/**
 * @brief Check if device is using pairing seed
 * @param sldev Device structure to check
 * @return true if using pairing seed
 */
bool is_using_pairing_seed(sramlink_device_t *sldev);

/**
 * @brief Scramble pairing data and generate new keys
 * @param sldev Device structure to scramble
 */
void sramlink_device_scramble_pairing(sramlink_device_t *sldev);

/**
 * @brief Locate paired device by type
 * @param sld Device structure
 * @param device_type Type to find
 * @param paired_device_index Output: index if found
 * @return true if found
 */
bool locate_paired_device_type(sramlink_device_t *sld, uint8_t device_type,
                               uint8_t *paired_device_index);

/**
 * @brief Locate paired device by ID
 * @param sld Device structure
 * @param device_id ID to find
 * @param paired_device_index Output: index if found
 * @return true if found
 */
bool locate_paired_device_by_id(sramlink_device_t *sld, uint32_t device_id,
                                uint8_t *paired_device_index);

/**
 * @brief Locate paired device by type and ID
 * @param sld Device structure
 * @param device_type Type to match
 * @param device_id ID to match
 * @param paired_device_index Output: index if found
 * @return true if found
 */
bool locate_paired_device(sramlink_device_t *sld, uint8_t device_type,
                          uint32_t device_id, uint8_t *paired_device_index);

/**
 * @brief Erase a paired device record
 * @param sld Device structure
 * @param idx Index to erase
 */
void erase_paired_data(sramlink_device_t *sld, uint8_t idx);

/**
 * @brief Erase all paired device records
 * @param sld Device structure
 */
void erase_all_pairings(sramlink_device_t *sld);

/**
 * @brief Find a free pairing index
 * @param sld Device structure
 * @param index Output: free index if found
 * @return true if free slot found
 */
bool locate_free_pairing_index(sramlink_device_t *sld, uint8_t *index);

/**
 * @brief Verify rolling code from received message
 * @param sld Device structure
 * @param header Message header
 * @param rolling_code Rolling code to verify
 * @param pinned_distance Output: distance from expected
 * @param paired_device_index Index of paired device
 * @return true if valid
 */
bool verify_rolling_code(sramlink_device_t *sld, slmsg_header_t *header,
                         uint32_t rolling_code, uint8_t *pinned_distance,
                         uint8_t paired_device_index);

/**
 * @brief Implicitly pair a new device
 * @param sld Device structure
 * @param header Message header
 * @param rolling_code Rolling code from message
 * @return true if paired successfully
 */
bool pair_device_implicitly(sramlink_device_t *sld, slmsg_header_t *header,
                            uint32_t rolling_code);

/**
 * @brief Save paired device data
 * @param sld Device structure
 * @param device_type Device type
 * @param device_id Device ID
 * @param rolling_code Rolling code
 * @param model_id Model ID
 * @param app_version App version
 * @return true if saved
 */
bool save_paired_device_data(sramlink_device_t *sld, uint8_t device_type,
                             uint32_t device_id, uint32_t rolling_code,
                             uint16_t model_id, uint32_t app_version);

/**
 * @brief Update firmware version for paired device type
 * @param sld Device structure
 * @param device_type Device type to update
 * @param firmware_version New firmware version
 * @return true if updated
 */
bool update_paired_device_type_firmware_version(sramlink_device_t *sld,
                                                uint8_t device_type,
                                                uint32_t firmware_version);

/**
 * @brief Get next rolling code
 * @param sld Device structure
 * @return Next rolling code
 */
uint32_t sramlink_device_get_next_rc(sramlink_device_t *sld);

/**
 * @brief Set rolling code
 * @param sld Device structure
 * @param new_rc New rolling code
 */
void sramlink_device_set_rc(sramlink_device_t *sld, uint32_t new_rc);

/**
 * @brief Generate new random master key
 * @param sld Device structure
 */
void sramlink_device_new_master_key(sramlink_device_t *sld);

/**
 * @brief Check if this device was the pairing master
 * @param sld Device structure
 * @return true if master
 */
bool sramlink_device_am_i_master(sramlink_device_t *sld);

/**
 * @brief Set device type
 * @param sld Device structure
 * @param device_type New device type
 * @return true if set
 */
bool sramlink_device_set_dtype(sramlink_device_t *sld, uint8_t device_type);

/**
 * @brief Check if paired rolling codes need to be stored
 * @param sld Device structure
 * @return true if need store
 */
bool sramlink_device_paired_rolling_codes_need_store(sramlink_device_t *sld);

/**
 * @brief Store paired rolling codes to NVS
 * @param sld Device structure
 */
void sramlink_device_store_paired_rolling_codes(sramlink_device_t *sld);

/**
 * @brief Check if device data needs to be stored
 * @param sld Device structure
 * @return true if need store
 */
bool sramlink_device_needs_store(sramlink_device_t *sld);

/**
 * @brief Set a paired device entry
 * @param sld Device structure
 * @param index Index to set
 * @param data Data to set
 */
void set_paired_device(sramlink_device_t *sld, uint8_t index,
                       sramlink_paired_data_t *data);

/**
 * @brief Print device info for debugging
 * @param sld Device structure
 */
void print_sramlink_device(sramlink_device_t *sld);
