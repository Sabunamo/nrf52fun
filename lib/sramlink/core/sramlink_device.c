/**
 * @file sramlink_device.c
 * @brief SRAMLink Device State Management for Zephyr
 *
 * Uses Zephyr NVS for persistent storage.
 */

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/random/random.h>

#if defined(CONFIG_NVS)
#include <zephyr/fs/nvs.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/drivers/flash.h>
#endif

#include "sramlink.h"
#include "sramlink_device.h"
#include "../messages/sramlink_messages.h"

LOG_MODULE_REGISTER(sramlink_device, CONFIG_LOG_DEFAULT_LEVEL);

/* NVS IDs */
#define NVS_ID_DEVICE_DATA      1
#define NVS_ID_PAIRED_RC        2
#define NVS_ID_ROLLING_CODE     3

/* NVS instance */
#if defined(CONFIG_NVS)
static struct nvs_fs nvs;
static bool nvs_initialized = false;
#endif

void sramlink_device_init(void)
{
#if defined(CONFIG_NVS)
    if (nvs_initialized) {
        return;
    }

    int rc;
    struct flash_pages_info info;

    /* Define the NVS file system by settings with:
     * sector_size equal to the page size,
     * 3 sectors
     * starting at the end of flash minus 3 pages
     */
    nvs.flash_device = FIXED_PARTITION_DEVICE(storage_partition);
    if (nvs.flash_device == NULL) {
        LOG_ERR("NVS: Flash device not found");
        return;
    }

    nvs.offset = FIXED_PARTITION_OFFSET(storage_partition);
    rc = flash_get_page_info_by_offs(nvs.flash_device, nvs.offset, &info);
    if (rc) {
        LOG_ERR("NVS: Unable to get page info");
        return;
    }
    nvs.sector_size = info.size;
    nvs.sector_count = 3U;

    rc = nvs_mount(&nvs);
    if (rc) {
        LOG_ERR("NVS: Flash init failed: %d", rc);
        return;
    }

    nvs_initialized = true;
    LOG_INF("NVS initialized for SRAMLink device storage");
#endif
}

void sramlink_device_load(sramlink_device_t *sld)
{
    if (sld == NULL) {
        return;
    }

#if defined(CONFIG_NVS)
    if (!nvs_initialized) {
        sramlink_device_init();
    }

    if (!nvs_initialized) {
        LOG_WRN("NVS not available, using defaults");
        memset(sld, 0, sizeof(sramlink_device_t));
        return;
    }

    /* Load device data */
    int rc = nvs_read(&nvs, NVS_ID_DEVICE_DATA, sld, sizeof(sramlink_device_t));
    if (rc <= 0) {
        LOG_WRN("No device data in NVS, using defaults");
        memset(sld, 0, sizeof(sramlink_device_t));

        /* Generate a random device ID if none exists */
        sld->device_id = sys_rand32_get();
        sld->radio_channel = 15;  /* Default channel */
        return;
    }

    /* Load paired rolling codes */
    uint32_t paired_rc[MAX_PAIRING_RECORDS];
    rc = nvs_read(&nvs, NVS_ID_PAIRED_RC, paired_rc, sizeof(paired_rc));
    if (rc > 0) {
        for (int i = 0; i < MAX_PAIRING_RECORDS; i++) {
            sld->paired_devices[i].rolling_code = paired_rc[i];
        }
    }

    /* Load current rolling code */
    uint32_t current_rc;
    rc = nvs_read(&nvs, NVS_ID_ROLLING_CODE, &current_rc, sizeof(current_rc));
    if (rc > 0) {
        sld->rolling_code = current_rc;
    }

    /* Clear store flags */
    sld->flags &= ~(1 << SLDEVICE_FLAG_BIT_PAIRED_RC_NEEDS_STORE);
    sld->flags &= ~(1 << SLDEVICE_FLAG_BIT_SLDEVICE_NEEDS_STORE);

    LOG_INF("Device data loaded, ID: 0x%08X, Channel: %d",
            sld->device_id, sld->radio_channel);
#else
    memset(sld, 0, sizeof(sramlink_device_t));
    sld->device_id = sys_rand32_get();
    sld->radio_channel = 15;
#endif
}

void sramlink_device_store(sramlink_device_t *sld)
{
    if (sld == NULL) {
        return;
    }

#if defined(CONFIG_NVS)
    if (!nvs_initialized) {
        LOG_ERR("NVS not initialized, cannot store");
        return;
    }

    /* Store device data */
    int rc = nvs_write(&nvs, NVS_ID_DEVICE_DATA, sld, sizeof(sramlink_device_t));
    if (rc < 0) {
        LOG_ERR("Failed to write device data: %d", rc);
        return;
    }

    /* Store paired rolling codes */
    sramlink_device_store_paired_rolling_codes(sld);

    /* Store current rolling code */
    rc = nvs_write(&nvs, NVS_ID_ROLLING_CODE, &sld->rolling_code,
                   sizeof(sld->rolling_code));
    if (rc < 0) {
        LOG_ERR("Failed to write rolling code: %d", rc);
    }

    /* Clear store flags */
    sld->flags &= ~(1 << SLDEVICE_FLAG_BIT_SLDEVICE_NEEDS_STORE);

    LOG_DBG("Device data stored");
#endif
}

bool is_using_pairing_seed(sramlink_device_t *sldev)
{
    if (sldev == NULL) {
        return false;
    }

    if (sldev->device_id != sldev->master_id) {
        return false;
    }

    if (sldev->radio_channel != PAIRING_SEED_RADIO_CHANNEL) {
        return false;
    }

    /* Check if master key matches device ID pattern */
    for (int i = 0; i < 12; i++) {
        if (sldev->master_key[i] != 0) {
            return false;
        }
    }

    if ((sldev->master_key[12] != ((sldev->device_id >> 24) & 0xff)) ||
        (sldev->master_key[13] != ((sldev->device_id >> 16) & 0xff)) ||
        (sldev->master_key[14] != ((sldev->device_id >> 8) & 0xff)) ||
        (sldev->master_key[15] != (sldev->device_id & 0xff))) {
        return false;
    }

    return true;
}

void sramlink_device_scramble_pairing(sramlink_device_t *sldev)
{
    if (sldev == NULL) {
        return;
    }

    erase_all_pairings(sldev);

    /* Generate random master ID */
    sldev->master_id = sys_rand32_get();

    /* Generate random master key */
    sys_rand_get(sldev->master_key, 16);

    /* Generate random channel (11-26) */
    sldev->radio_channel = (sys_rand32_get() % 16) + 11;

    LOG_INF("Scrambled pairing: channel=%d, master_id=0x%08X",
            sldev->radio_channel, sldev->master_id);

    sramlink_device_store(sldev);
}

bool locate_paired_device_type(sramlink_device_t *sld, uint8_t device_type,
                               uint8_t *paired_device_index)
{
    if (sld == NULL) {
        return false;
    }

    for (int i = 0; i < MAX_PAIRING_RECORDS; i++) {
        if (sld->paired_devices[i].active &&
            sld->paired_devices[i].device_type == device_type) {
            if (paired_device_index) {
                *paired_device_index = i;
            }
            return true;
        }
    }
    return false;
}

bool locate_paired_device_by_id(sramlink_device_t *sld, uint32_t device_id,
                                uint8_t *paired_device_index)
{
    if (sld == NULL) {
        return false;
    }

    for (int i = 0; i < MAX_PAIRING_RECORDS; i++) {
        if (sld->paired_devices[i].active &&
            sld->paired_devices[i].device_id == device_id) {
            if (paired_device_index) {
                *paired_device_index = i;
            }
            return true;
        }
    }
    return false;
}

bool locate_paired_device(sramlink_device_t *sld, uint8_t device_type,
                          uint32_t device_id, uint8_t *paired_device_index)
{
    if (sld == NULL) {
        return false;
    }

    for (int i = 0; i < MAX_PAIRING_RECORDS; i++) {
        if (sld->paired_devices[i].active &&
            sld->paired_devices[i].device_type == device_type &&
            sld->paired_devices[i].device_id == device_id) {
            if (paired_device_index) {
                *paired_device_index = i;
            }
            return true;
        }
    }
    return false;
}

void erase_paired_data(sramlink_device_t *sld, uint8_t idx)
{
    if (sld == NULL || idx >= MAX_PAIRING_RECORDS) {
        return;
    }

    LOG_DBG("Erasing paired device at index %d", idx);
    memset(&sld->paired_devices[idx], 0, sizeof(sramlink_paired_data_t));
}

void erase_all_pairings(sramlink_device_t *sld)
{
    if (sld == NULL) {
        return;
    }

    for (int i = 0; i < MAX_PAIRING_RECORDS; i++) {
        erase_paired_data(sld, i);
    }
}

bool locate_free_pairing_index(sramlink_device_t *sld, uint8_t *index)
{
    if (sld == NULL || index == NULL) {
        return false;
    }

    for (int i = 0; i < MAX_PAIRING_RECORDS; i++) {
        if (!sld->paired_devices[i].active) {
            *index = i;
            return true;
        }
    }
    return false;
}

bool verify_rolling_code(sramlink_device_t *sld, slmsg_header_t *header,
                         uint32_t rolling_code, uint8_t *pinned_distance,
                         uint8_t paired_device_index)
{
    if (sld == NULL || paired_device_index >= MAX_PAIRING_RECORDS) {
        return false;
    }

    uint32_t message_distance = rolling_code -
        sld->paired_devices[paired_device_index].rolling_code;

    if (message_distance < MESSAGE_DISTANCE_LIMIT) {
        /* Update rolling code in memory */
        sld->paired_devices[paired_device_index].rolling_code = rolling_code;

        if (pinned_distance) {
            *pinned_distance = (message_distance < 256) ? message_distance : 255;
        }

        /* Check if we need to store */
        if (message_distance >= MESSAGE_DISTANCE_LIMIT / 2) {
            sld->flags |= 1 << SLDEVICE_FLAG_BIT_PAIRED_RC_NEEDS_STORE;
        }

        return true;
    }
    return false;
}

bool pair_device_implicitly(sramlink_device_t *sld, slmsg_header_t *header,
                            uint32_t rolling_code)
{
    if (sld == NULL || header == NULL) {
        return false;
    }

    uint8_t paired_device_index;
    if (locate_free_pairing_index(sld, &paired_device_index)) {
        erase_paired_data(sld, paired_device_index);

        sld->paired_devices[paired_device_index].active = true;
        sld->paired_devices[paired_device_index].device_type = header->device_type;
        sld->paired_devices[paired_device_index].device_id = header->device_id;
        sld->paired_devices[paired_device_index].rolling_code = rolling_code;

        sld->flags |= 1 << SLDEVICE_FLAG_BIT_PAIRED_RC_NEEDS_STORE;
        sld->flags |= 1 << SLDEVICE_FLAG_BIT_SLDEVICE_NEEDS_STORE;

        LOG_INF("Implicit pairing: type=%d, id=0x%08X",
                header->device_type, header->device_id);
        return true;
    }
    return false;
}

bool save_paired_device_data(sramlink_device_t *sld, uint8_t device_type,
                             uint32_t device_id, uint32_t rolling_code,
                             uint16_t model_id, uint32_t app_version)
{
    if (sld == NULL) {
        return false;
    }

    uint8_t index;
    if (locate_paired_device(sld, device_type, device_id, &index)) {
        sld->paired_devices[index].rolling_code = rolling_code;
        sld->paired_devices[index].model_id = model_id;
        sld->paired_devices[index].app_version = app_version;
        return true;
    }
    return false;
}

bool update_paired_device_type_firmware_version(sramlink_device_t *sld,
                                                uint8_t device_type,
                                                uint32_t firmware_version)
{
    if (sld == NULL) {
        return false;
    }

    uint8_t i;
    if (locate_paired_device_type(sld, device_type, &i)) {
        if (sld->paired_devices[i].app_version != firmware_version) {
            sld->paired_devices[i].app_version = firmware_version;
            sld->flags |= 1 << SLDEVICE_FLAG_BIT_SLDEVICE_NEEDS_STORE;
        }
        return true;
    }
    return false;
}

uint32_t sramlink_device_get_next_rc(sramlink_device_t *sld)
{
    if (sld == NULL) {
        return 0;
    }

    sld->rolling_code++;
    return sld->rolling_code;
}

void sramlink_device_set_rc(sramlink_device_t *sld, uint32_t new_rc)
{
    if (sld == NULL) {
        return;
    }

    sld->rolling_code = new_rc;
}

void sramlink_device_new_master_key(sramlink_device_t *sld)
{
    if (sld == NULL) {
        return;
    }

    /* Generate random 16-byte key */
    sys_rand_get(sld->master_key, 16);

    /* Make sure it doesn't match pairing seed pattern */
    while (is_using_pairing_seed(sld)) {
        sys_rand_get(sld->master_key, 16);
    }

    LOG_DBG("Generated new master key");
}

bool sramlink_device_am_i_master(sramlink_device_t *sld)
{
    if (sld == NULL) {
        return false;
    }
    return (sld->master_id == sld->device_id);
}

bool sramlink_device_set_dtype(sramlink_device_t *sld, uint8_t device_type)
{
    if (sld == NULL) {
        return false;
    }

    sld->device_type = device_type;
    sld->flags |= 1 << SLDEVICE_FLAG_BIT_SLDEVICE_NEEDS_STORE;
    return true;
}

bool sramlink_device_paired_rolling_codes_need_store(sramlink_device_t *sld)
{
    if (sld == NULL) {
        return false;
    }
    return (sld->flags & (1 << SLDEVICE_FLAG_BIT_PAIRED_RC_NEEDS_STORE)) != 0;
}

void sramlink_device_store_paired_rolling_codes(sramlink_device_t *sld)
{
    if (sld == NULL) {
        return;
    }

#if defined(CONFIG_NVS)
    if (!nvs_initialized) {
        return;
    }

    if (sramlink_device_paired_rolling_codes_need_store(sld)) {
        uint32_t paired_rc[MAX_PAIRING_RECORDS];
        for (int i = 0; i < MAX_PAIRING_RECORDS; i++) {
            paired_rc[i] = sld->paired_devices[i].rolling_code;
        }

        int rc = nvs_write(&nvs, NVS_ID_PAIRED_RC, paired_rc, sizeof(paired_rc));
        if (rc < 0) {
            LOG_ERR("Failed to write paired rolling codes: %d", rc);
        } else {
            sld->flags &= ~(1 << SLDEVICE_FLAG_BIT_PAIRED_RC_NEEDS_STORE);
            LOG_DBG("Paired rolling codes stored");
        }
    }
#endif
}

bool sramlink_device_needs_store(sramlink_device_t *sld)
{
    if (sld == NULL) {
        return false;
    }
    return (sld->flags & (1 << SLDEVICE_FLAG_BIT_SLDEVICE_NEEDS_STORE)) != 0;
}

void set_paired_device(sramlink_device_t *sld, uint8_t index,
                       sramlink_paired_data_t *data)
{
    if (sld == NULL || data == NULL || index >= MAX_PAIRING_RECORDS) {
        return;
    }
    sld->paired_devices[index] = *data;
}

void print_sramlink_device(sramlink_device_t *sld)
{
    if (sld == NULL) {
        return;
    }

    LOG_INF("SRAMLink Device:");
    LOG_INF("  Device Type: %d", sld->device_type);
    LOG_INF("  Device ID: 0x%08X", sld->device_id);
    LOG_INF("  Master ID: 0x%08X", sld->master_id);
    LOG_INF("  Rolling Code: %u", sld->rolling_code);
    LOG_INF("  Radio Channel: %d", sld->radio_channel);

    for (int i = 0; i < MAX_PAIRING_RECORDS; i++) {
        if (sld->paired_devices[i].active) {
            LOG_INF("  Paired[%d]: type=%d, id=0x%08X, rc=%u",
                    i, sld->paired_devices[i].device_type,
                    sld->paired_devices[i].device_id,
                    sld->paired_devices[i].rolling_code);
        }
    }
}
