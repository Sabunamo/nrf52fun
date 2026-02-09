/**
 * @file sramlink_messages.c
 * @brief SRAMLink Message Packing/Unpacking for Zephyr
 *
 * Ported from bambam for nRF Connect SDK.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <zephyr/logging/log.h>

#include "sramlink_messages.h"
#include "sramlink_messages_common.h"
#include "../util/packer.h"
#include "../drivers/sl_radio.h"
#include "../core/sramlink_device.h"

LOG_MODULE_REGISTER(slmsg, CONFIG_SRAMLINK_LOG_LEVEL);

/** Default protocol version for packing/unpacking */
static uint8_t default_version = SL_VERSION_2_0;

/** Bitmask for targeted message */
#define SRAMLINK_TARGETED_MASK 0x80

/** Header length for v2.0 - local constant to avoid macro recursion */
#define SL_HDR_LEN 11

/** Pointer to current header during pack/unpack */
static slmsg_header_t *m_p_current_header;

/*---------------------------------------------------------------------------
 * v2.0 Message Pack/Unpack Functions
 *---------------------------------------------------------------------------*/

static void pack_v2_0_buttons(void *data_struct, sl_radio_frame_t *frame)
{
    slmsg_v2_0_buttons_t *msg = data_struct;
    pack_be_uint32(frame->data, &frame->length, msg->rolling_code);
    pack_uint8(frame->data, &frame->length, msg->button_mask);
    pack_uint8(frame->data, &frame->length, msg->flags);
}

static bool unpack_v2_0_buttons(sl_radio_frame_t *frame, void *data_struct)
{
    slmsg_v2_0_buttons_t *msg = data_struct;
    uint8_t offset = SL_HDR_LEN;

    msg->rolling_code = unpack_be_uint32(frame->data, &offset);
    msg->button_mask = unpack_uint8(frame->data, &offset);
    msg->flags = unpack_uint8(frame->data, &offset);

    return (offset <= frame->length);
}

static void pack_v2_0_status_request(void *data_struct, sl_radio_frame_t *frame)
{
    slmsg_v2_0_status_request_t *msg = data_struct;
    pack_be_uint32(frame->data, &frame->length, msg->rolling_code);
    pack_uint8(frame->data, &frame->length, msg->request_type);
}

static bool unpack_v2_0_status_request(sl_radio_frame_t *frame, void *data_struct)
{
    slmsg_v2_0_status_request_t *msg = data_struct;
    uint8_t offset = SL_HDR_LEN;

    msg->rolling_code = unpack_be_uint32(frame->data, &offset);
    msg->request_type = unpack_uint8(frame->data, &offset);

    return (offset <= frame->length);
}

static void pack_v2_0_status_report(void *data_struct, sl_radio_frame_t *frame)
{
    slmsg_v2_0_status_report_t *msg = data_struct;
    pack_be_uint32(frame->data, &frame->length, msg->rolling_code);
    pack_uint8(frame->data, &frame->length, msg->index_a);
    pack_uint8(frame->data, &frame->length, msg->index_b);
    pack_uint8(frame->data, &frame->length, msg->index_c);
    pack_uint8(frame->data, &frame->length, msg->flags);
    pack_be_uint16(frame->data, &frame->length, msg->battery_voltage);
    pack_uint8(frame->data, &frame->length, msg->battery_status);
}

static bool unpack_v2_0_status_report(sl_radio_frame_t *frame, void *data_struct)
{
    slmsg_v2_0_status_report_t *msg = data_struct;
    uint8_t offset = SL_HDR_LEN;

    msg->rolling_code = unpack_be_uint32(frame->data, &offset);
    msg->index_a = unpack_uint8(frame->data, &offset);
    msg->index_b = unpack_uint8(frame->data, &offset);
    msg->index_c = unpack_uint8(frame->data, &offset);
    msg->flags = unpack_uint8(frame->data, &offset);
    msg->battery_voltage = unpack_be_uint16(frame->data, &offset);
    msg->battery_status = unpack_uint8(frame->data, &offset);

    return (offset <= frame->length);
}

static void pack_v2_0_device_info(void *data_struct, sl_radio_frame_t *frame)
{
    slmsg_v2_0_device_info_t *msg = data_struct;
    pack_be_uint32(frame->data, &frame->length, msg->rolling_code);
    pack_be_uint32(frame->data, &frame->length, msg->app_version);
    pack_be_uint16(frame->data, &frame->length, msg->model_id);
    pack_uint8(frame->data, &frame->length, msg->protocol_bridge_version);
}

static bool unpack_v2_0_device_info(sl_radio_frame_t *frame, void *data_struct)
{
    slmsg_v2_0_device_info_t *msg = data_struct;
    uint8_t offset = SL_HDR_LEN;

    msg->rolling_code = unpack_be_uint32(frame->data, &offset);
    msg->app_version = unpack_be_uint32(frame->data, &offset);
    msg->model_id = unpack_be_uint16(frame->data, &offset);

    /* Optional field */
    if ((frame->length - SL_HDR_LEN) >= 11) {
        msg->protocol_bridge_version = unpack_uint8(frame->data, &offset);
    } else {
        msg->protocol_bridge_version = 0;
    }

    return (offset <= frame->length);
}

static void pack_v2_0_test(void *data_struct, sl_radio_frame_t *frame)
{
    slmsg_v2_0_test_t *msg = data_struct;
    pack_be_uint32(frame->data, &frame->length, msg->rolling_code);
    pack_uint8(frame->data, &frame->length, msg->payload_len);

    for (uint8_t i = 0; i < msg->payload_len; i++) {
        pack_uint8(frame->data, &frame->length, msg->payload[i]);
    }
}

static bool unpack_v2_0_test(sl_radio_frame_t *frame, void *data_struct)
{
    slmsg_v2_0_test_t *msg = data_struct;
    uint8_t offset = SL_HDR_LEN;

    msg->rolling_code = unpack_be_uint32(frame->data, &offset);
    msg->payload_len = unpack_uint8(frame->data, &offset);

    for (uint8_t i = 0; i < msg->payload_len && i < SL_TEST_PAYLOAD_MAX; i++) {
        msg->payload[i] = unpack_uint8(frame->data, &offset);
    }

    return true;
}

static void pack_v2_0_network_settings(void *data_struct, sl_radio_frame_t *frame)
{
    slmsg_v2_0_network_settings_t *msg = data_struct;
    pack_be_uint32(frame->data, &frame->length, msg->rolling_code);
    pack_be_uint32(frame->data, &frame->length, msg->network_id);
    pack_uint8(frame->data, &frame->length, msg->network_channel);
    pack_uint8_array(frame->data, &frame->length, msg->network_key, 16);
}

static bool unpack_v2_0_network_settings(sl_radio_frame_t *frame, void *data_struct)
{
    slmsg_v2_0_network_settings_t *msg = data_struct;
    uint8_t offset = SL_HDR_LEN;

    msg->rolling_code = unpack_be_uint32(frame->data, &offset);
    msg->network_id = unpack_be_uint32(frame->data, &offset);
    msg->network_channel = unpack_uint8(frame->data, &offset);
    unpack_uint8_array(frame->data, &offset, msg->network_key, 16);

    return (offset <= frame->length);
}

static void pack_v2_0_public_key_share(void *data_struct, sl_radio_frame_t *frame)
{
    slmsg_v2_0_public_key_share_t *msg = data_struct;
    pack_uint8(frame->data, &frame->length, msg->adv_channel);
    pack_uint8_array(frame->data, &frame->length, msg->public_key, 16);
    pack_uint8_array(frame->data, &frame->length, msg->aes_challenge_response, 16);
}

static bool unpack_v2_0_public_key_share(sl_radio_frame_t *frame, void *data_struct)
{
    slmsg_v2_0_public_key_share_t *msg = data_struct;
    uint8_t offset = SL_HDR_LEN;

    msg->adv_channel = unpack_uint8(frame->data, &offset);
    unpack_uint8_array(frame->data, &offset, msg->public_key, 16);
    unpack_uint8_array(frame->data, &offset, msg->aes_challenge_response, 16);

    return (offset <= frame->length);
}

/*---------------------------------------------------------------------------
 * Message Definition Table
 *---------------------------------------------------------------------------*/

typedef void (*pack_func_t)(void *data_struct, sl_radio_frame_t *frame);
typedef bool (*unpack_func_t)(sl_radio_frame_t *frame, void *data_struct);
typedef bool (*crypt_func_t)(uint8_t *key, sl_radio_frame_t *frame);

typedef struct {
    uint8_t msg_type;
    bool has_rolling_code;
    bool has_payload;
    pack_func_t pack;
    unpack_func_t unpack;
    crypt_func_t encrypt;
    crypt_func_t decrypt;
} message_definition_t;

/* Forward declarations for crypto */
static bool slcrypto_v2_0_encrypt_frame(uint8_t *key, sl_radio_frame_t *frame);
static bool slcrypto_v2_0_decrypt_frame(uint8_t *key, sl_radio_frame_t *frame);

static const message_definition_t message_table_v2_0[] = {
    /* NOP - no payload */
    {
        .msg_type = SLMSGTYPE_NOP,
        .has_rolling_code = false,
        .has_payload = false,
        .pack = NULL,
        .unpack = NULL,
        .encrypt = NULL,
        .decrypt = NULL,
    },
    /* BUTTONS */
    {
        .msg_type = SLMSGTYPE_BUTTONS,
        .has_rolling_code = true,
        .has_payload = true,
        .pack = pack_v2_0_buttons,
        .unpack = unpack_v2_0_buttons,
        .encrypt = slcrypto_v2_0_encrypt_frame,
        .decrypt = slcrypto_v2_0_decrypt_frame,
    },
    /* STATUS_REQUEST */
    {
        .msg_type = SLMSGTYPE_STATUS_REQUEST,
        .has_rolling_code = true,
        .has_payload = true,
        .pack = pack_v2_0_status_request,
        .unpack = unpack_v2_0_status_request,
        .encrypt = slcrypto_v2_0_encrypt_frame,
        .decrypt = slcrypto_v2_0_decrypt_frame,
    },
    /* STATUS_REPORT */
    {
        .msg_type = SLMSGTYPE_STATUS_REPORT,
        .has_rolling_code = true,
        .has_payload = true,
        .pack = pack_v2_0_status_report,
        .unpack = unpack_v2_0_status_report,
        .encrypt = slcrypto_v2_0_encrypt_frame,
        .decrypt = slcrypto_v2_0_decrypt_frame,
    },
    /* DEVICE_INFO */
    {
        .msg_type = SLMSGTYPE_DEVICE_INFO,
        .has_rolling_code = true,
        .has_payload = true,
        .pack = pack_v2_0_device_info,
        .unpack = unpack_v2_0_device_info,
        .encrypt = slcrypto_v2_0_encrypt_frame,
        .decrypt = slcrypto_v2_0_decrypt_frame,
    },
    /* TEST - no encryption */
    {
        .msg_type = SLMSGTYPE_TEST,
        .has_rolling_code = true,
        .has_payload = true,
        .pack = pack_v2_0_test,
        .unpack = unpack_v2_0_test,
        .encrypt = NULL,
        .decrypt = NULL,
    },
    /* PUBLIC_KEY_SHARE - no encryption, no rolling code */
    {
        .msg_type = SLMSGTYPE_PUBLIC_KEY_SHARE,
        .has_rolling_code = false,
        .has_payload = true,
        .pack = pack_v2_0_public_key_share,
        .unpack = unpack_v2_0_public_key_share,
        .encrypt = NULL,
        .decrypt = NULL,
    },
    /* NETWORK_SETTINGS */
    {
        .msg_type = SLMSGTYPE_NETWORK_SETTINGS,
        .has_rolling_code = true,
        .has_payload = true,
        .pack = pack_v2_0_network_settings,
        .unpack = unpack_v2_0_network_settings,
        .encrypt = slcrypto_v2_0_encrypt_frame,
        .decrypt = slcrypto_v2_0_decrypt_frame,
    },
};

#define NUM_V2_0_MESSAGES (sizeof(message_table_v2_0) / sizeof(message_table_v2_0[0]))

/*---------------------------------------------------------------------------
 * Message Definition Lookup
 *---------------------------------------------------------------------------*/

static const message_definition_t *get_msg_definition(uint8_t version, uint8_t msg_type)
{
    /* Mask off targeted bit */
    msg_type = msg_type & (~SRAMLINK_TARGETED_MASK);

    if (version == SL_VERSION_2_0) {
        for (size_t i = 0; i < NUM_V2_0_MESSAGES; i++) {
            if (message_table_v2_0[i].msg_type == msg_type) {
                return &message_table_v2_0[i];
            }
        }
    }

    LOG_ERR("Message type %u not found in version 0x%02X", msg_type, version);
    return NULL;
}

/*---------------------------------------------------------------------------
 * Encryption/Decryption using hardware AES via sl_radio
 *---------------------------------------------------------------------------*/

#define SLV2_0_EAX_TAG_SIZE     4

static bool slcrypto_v2_0_encrypt_frame(uint8_t *key, sl_radio_frame_t *frame)
{
    if (key == NULL) {
        return true; /* No key means no encryption needed */
    }

    if (frame == NULL) {
        LOG_ERR("No frame passed to encrypt");
        return false;
    }

    /* Set AES key */
    sl_radio_set_aes_key(key);

    /* Use hardware AES from radio driver */
    uint8_t payload_len = frame->length - SL_HDR_LEN;
    uint8_t padded_len = ((payload_len + 15) / 16) * 16;

    /* Pad with zeros if needed */
    if (padded_len > payload_len) {
        memset(&frame->data[frame->length], 0, padded_len - payload_len);
    }

    /* Encrypt payload in place using hardware AES-ECB (direction=0 for encrypt) */
    uint8_t temp[16];
    for (uint8_t i = 0; i < padded_len; i += 16) {
        sl_radio_aes_encrypt(&frame->data[SL_HDR_LEN + i], temp, 0);
        memcpy(&frame->data[SL_HDR_LEN + i], temp, 16);
    }

    /* Update length with padding and tag space */
    frame->length = SL_HDR_LEN + padded_len + SLV2_0_EAX_TAG_SIZE;

    return true;
}

static bool slcrypto_v2_0_decrypt_frame(uint8_t *key, sl_radio_frame_t *frame)
{
    if (key == NULL) {
        return true; /* No key means no decryption needed */
    }

    if (frame == NULL) {
        LOG_ERR("No frame passed to decrypt");
        return false;
    }

    /* Set AES key */
    sl_radio_set_aes_key(key);

    /* Calculate encrypted payload length (minus tag) */
    uint8_t cipher_len = frame->length - SL_HDR_LEN - SLV2_0_EAX_TAG_SIZE;
    uint8_t padded_len = ((cipher_len + 15) / 16) * 16;

    /* Decrypt payload in place using hardware AES-ECB (direction=1 for decrypt) */
    uint8_t temp[16];
    for (uint8_t i = 0; i < padded_len; i += 16) {
        sl_radio_aes_encrypt(&frame->data[SL_HDR_LEN + i], temp, 1);
        memcpy(&frame->data[SL_HDR_LEN + i], temp, 16);
    }

    /* Update length (remove padding and tag) */
    frame->length = SL_HDR_LEN + cipher_len;

    return true;
}

/*---------------------------------------------------------------------------
 * Public API
 *---------------------------------------------------------------------------*/

uint8_t slmsg_get_header_length(void)
{
    switch (default_version) {
    case SL_VERSION_1_1:
        return 10;
    case SL_VERSION_1_2:
    case SL_VERSION_2_0:
    default:
        return 11;
    }
}

void slmsg_pack_header(slmsg_header_t *header, sl_radio_frame_t *frame)
{
    frame->length = 0;

    if (default_version == SL_VERSION_1_2 || default_version == SL_VERSION_2_0) {
        pack_uint8(frame->data, &frame->length, header->version);
    }

    pack_be_uint32(frame->data, &frame->length, header->device_id);
    pack_be_uint32(frame->data, &frame->length, header->master_id);
    pack_uint8(frame->data, &frame->length, header->device_type);

    if (header->is_targeted) {
        pack_uint8(frame->data, &frame->length,
                   SRAMLINK_TARGETED_MASK | header->message_type);
    } else {
        pack_uint8(frame->data, &frame->length, header->message_type);
    }
}

void slmsg_unpack_header(sl_radio_frame_t *frame, slmsg_header_t *header)
{
    uint8_t offset = 0;

    if (default_version == SL_VERSION_1_2 || default_version == SL_VERSION_2_0) {
        header->version = unpack_uint8(frame->data, &offset);
    } else {
        header->version = default_version;
    }

    header->device_id = unpack_be_uint32(frame->data, &offset);
    header->master_id = unpack_be_uint32(frame->data, &offset);
    header->device_type = unpack_uint8(frame->data, &offset);
    header->message_type = unpack_uint8(frame->data, &offset);

    /* Handle targeting */
    if ((header->message_type & SRAMLINK_TARGETED_MASK) != 0) {
        header->message_type = header->message_type & (~SRAMLINK_TARGETED_MASK);
        header->is_targeted = true;
    } else {
        header->is_targeted = false;
    }
}

bool slmsg_pack_msg_by_version(uint8_t version, slmsg_header_t *header,
                               void *data_struct, uint8_t *key,
                               sl_radio_frame_t *frame)
{
    const message_definition_t *p_msg = get_msg_definition(version, header->message_type);

    if (p_msg == NULL) {
        return false;
    }

    /* Force version byte in header */
    header->version = version;

    slmsg_pack_header(header, frame);

    m_p_current_header = header;

    if (p_msg->has_payload && p_msg->pack != NULL) {
        p_msg->pack(data_struct, frame);

        if (p_msg->encrypt != NULL && key != NULL) {
            if (!p_msg->encrypt(key, frame)) {
                m_p_current_header = NULL;
                return false;
            }
        }
    }

    m_p_current_header = NULL;
    return true;
}

bool slmsg_pack_msg(slmsg_header_t *header, void *data_struct,
                    uint8_t *key, sl_radio_frame_t *frame)
{
    return slmsg_pack_msg_by_version(default_version, header, data_struct, key, frame);
}

bool slmsg_unpack_msg(sl_radio_frame_t *frame, uint8_t *key,
                      slmsg_header_t *header, void *data_struct)
{
    slmsg_unpack_header(frame, header);

    const message_definition_t *p_msg = get_msg_definition(header->version, header->message_type);

    if (p_msg == NULL) {
        LOG_ERR("Message type %u not found in version %u",
                header->message_type, header->version);
        return false;
    }

    if (!p_msg->has_payload) {
        return true; /* Already unpacked header */
    }

    m_p_current_header = header;

    if (p_msg->decrypt != NULL && key != NULL) {
        if (!p_msg->decrypt(key, frame)) {
            LOG_WRN("Message type %uv%u failed to decrypt",
                    header->message_type, header->version);
            m_p_current_header = NULL;
            return false;
        }
    }

    bool result = true;
    if (p_msg->unpack != NULL) {
        result = p_msg->unpack(frame, data_struct);
    }

    m_p_current_header = NULL;
    return result;
}

void slmsg_set_default_version(uint8_t version)
{
    if (version == SL_VERSION_1_1 || version == SL_VERSION_1_2 ||
        version == SL_VERSION_2_0) {
        default_version = version;
    }
}

uint8_t slmsg_get_default_version(void)
{
    return default_version;
}

bool slmsg_is_valid_version(uint8_t version)
{
    return (version == SL_VERSION_1_1 || version == SL_VERSION_1_2 ||
            version == SL_VERSION_2_0);
}

void slmsg_init_broadcast_header(sramlink_device_t *sldev, uint8_t msg_type,
                                 slmsg_header_t *header)
{
    header->version = SL_VERSION_INVALID;
    header->master_id = sldev->master_id;
    header->device_id = sldev->device_id;
    header->device_type = sldev->device_type;
    header->message_type = msg_type;
    header->is_targeted = false;
}

void slmsg_init_targeted_header(sramlink_device_t *sldev, uint32_t target_did,
                                uint8_t msg_type, slmsg_header_t *header)
{
    header->version = SL_VERSION_INVALID;
    header->master_id = target_did;
    header->device_id = sldev->device_id;
    header->device_type = sldev->device_type;
    header->message_type = msg_type;
    header->is_targeted = true;
}

void slmsg_init_global_broadcast_header(sramlink_device_t *sldev,
                                        uint8_t msg_type,
                                        slmsg_header_t *header)
{
    header->master_id = 0;
    header->device_id = sldev->device_id;
    header->device_type = sldev->device_type;
    header->message_type = msg_type;
    header->is_targeted = false;
}

bool slmsg_is_targeted_to_me(slmsg_header_t *header, sramlink_device_t *sldev)
{
    return (header->is_targeted && (header->master_id == sldev->device_id));
}

slmsg_header_t *slmsg_get_current_header(void)
{
    return m_p_current_header;
}

bool slmsg_unpack_rolling_code(sl_radio_frame_t *frame, uint32_t *rolling_code)
{
    uint8_t offset = SL_HDR_LEN;
    uint32_t rc = unpack_be_uint32(frame->data, &offset);

    if (offset <= frame->length) {
        *rolling_code = rc;
        return true;
    }
    return false;
}

bool slmsg_has_rolling_code(uint8_t version, slmsg_msg_type_t msg_type)
{
    if (version == SL_VERSION_DEFAULT) {
        version = default_version;
    }

    const message_definition_t *p_msg = get_msg_definition(version, msg_type);
    if (p_msg == NULL) {
        return false;
    }

    return p_msg->has_rolling_code;
}

bool slmsg_has_payload(uint8_t version, slmsg_msg_type_t msg_type)
{
    const message_definition_t *p_msg = get_msg_definition(version, msg_type);
    if (p_msg == NULL) {
        return false;
    }

    return p_msg->has_payload;
}
