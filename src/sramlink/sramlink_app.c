/**
 * @file sramlink_app.c
 * @brief SRAMLink Application Integration Layer
 *
 * High-level API for integrating SRAMLink with the INDOOR application.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "sramlink_app.h"
#include "../../lib/sramlink/core/sramlink.h"
#include "../../lib/sramlink/core/sramlink_device.h"
#include "../../lib/sramlink/messages/sramlink_messages.h"
#include "../../lib/sramlink/messages/sramlink_messages_common.h"
#include "../../lib/sramlink/drivers/sl_radio.h"

LOG_MODULE_REGISTER(sramlink_app, CONFIG_SRAMLINK_LOG_LEVEL);

/* Pairing timeout and TX interval */
#define PAIRING_TIMEOUT_MS      30000
#define PAIRING_TX_INTERVAL_MS  500

/* Application state */
static struct {
    bool initialized;
    bool running;
    bool pairing_mode;
    uint32_t pairing_start_time;
    uint32_t pairing_last_tx_time;
    bool pairing_complete;
    sramlink_device_t device;
    sramlink_app_config_t config;
} app_state;

/* RX frame buffer */
static sl_radio_frame_t rx_frame;

/* TX frame buffer */
static sl_radio_frame_t tx_frame;

/*---------------------------------------------------------------------------
 * Internal Functions
 *---------------------------------------------------------------------------*/

static void process_received_message(sl_radio_frame_t *frame, uint8_t lqi)
{
    slmsg_header_t header;
    slmsg_union_t msg_data;

    /* Unpack message */
    if (!slmsg_unpack_msg(frame, app_state.device.master_key, &header, &msg_data)) {
        /* PUBLIC_KEY_SHARE is unencrypted — try unpacking with NULL key */
        if (!slmsg_unpack_msg(frame, NULL, &header, &msg_data)) {
            LOG_WRN("Failed to unpack message");
            return;
        }
    }

    /* Handle PUBLIC_KEY_SHARE during pairing mode (before paired device check) */
    if (header.message_type == SLMSGTYPE_PUBLIC_KEY_SHARE) {
        if (!app_state.pairing_mode) {
            LOG_DBG("Ignoring PUBLIC_KEY_SHARE - not in pairing mode");
            return;
        }

        slmsg_v2_0_public_key_share_t *pks = &msg_data.v2_0.public_key_share;
        uint32_t sender_id = header.device_id;
        uint32_t my_id = app_state.device.device_id;

        LOG_INF("RX PUBLIC_KEY_SHARE from 0x%08X (my_id=0x%08X, channel=%u)",
                sender_id, my_id, pks->adv_channel);

        /* Extract sender's master_id from aes_challenge_response[0..3] (big-endian) */
        uint32_t sender_master_id = ((uint32_t)pks->aes_challenge_response[0] << 24) |
                                    ((uint32_t)pks->aes_challenge_response[1] << 16) |
                                    ((uint32_t)pks->aes_challenge_response[2] << 8) |
                                    ((uint32_t)pks->aes_challenge_response[3]);

        /* Lower device_id adopts the other's key (becomes "slave") */
        if (my_id < sender_id) {
            LOG_INF("Adopting key from 0x%08X (we are slave, lower device_id)", sender_id);
            memcpy(app_state.device.master_key, pks->public_key, 16);
            app_state.device.master_id = sender_master_id;
            app_state.device.radio_channel = pks->adv_channel;
        } else {
            LOG_INF("Keeping our key (we are master, higher device_id=0x%08X)", my_id);
        }

        /* Pair the sender device */
        save_paired_device_data(&app_state.device, header.device_type,
                                sender_id, 0, 0, 0);

        /* Store to NVS */
        sramlink_device_store(&app_state.device);

        /* Mark pairing complete and exit pairing mode */
        app_state.pairing_complete = true;
        LOG_INF("Pairing successful: 0x%08X <-> 0x%08X", my_id, sender_id);

        sramlink_app_stop_pairing();
        return;
    }

    /* Verify source device if not in pairing mode */
    if (!app_state.pairing_mode) {
        uint8_t paired_idx;
        if (!locate_paired_device_by_id(&app_state.device, header.device_id, &paired_idx)) {
            LOG_DBG("Message from unpaired device 0x%08X", header.device_id);

            /* Try implicit pairing for certain message types */
            if (header.message_type == SLMSGTYPE_BUTTONS ||
                header.message_type == SLMSGTYPE_STATUS_REPORT) {
                if (slmsg_has_rolling_code(header.version, header.message_type)) {
                    pair_device_implicitly(&app_state.device, &header,
                                          msg_data.rolling_code);
                }
            }
            return;
        }

        /* Verify rolling code */
        if (slmsg_has_rolling_code(header.version, header.message_type)) {
            uint8_t distance;
            if (!verify_rolling_code(&app_state.device, &header,
                                    msg_data.rolling_code, &distance, paired_idx)) {
                LOG_WRN("Invalid rolling code from 0x%08X", header.device_id);
                return;
            }
        }
    }

    /* Handle message by type */
    switch (header.message_type) {
    case SLMSGTYPE_BUTTONS:
        LOG_INF("RX BUTTONS from 0x%08X: mask=0x%02X flags=0x%02X",
                header.device_id,
                msg_data.v2_0.buttons.button_mask,
                msg_data.v2_0.buttons.flags);
        if (app_state.config.button_cb) {
            app_state.config.button_cb(header.device_id,
                                       msg_data.v2_0.buttons.button_mask,
                                       msg_data.v2_0.buttons.flags);
        }
        break;

    case SLMSGTYPE_STATUS_REQUEST:
        LOG_DBG("RX STATUS_REQUEST from 0x%08X type=%u",
                header.device_id,
                msg_data.v2_0.status_request.request_type);
        break;

    case SLMSGTYPE_STATUS_REPORT:
        LOG_INF("RX STATUS_REPORT from 0x%08X: %u/%u/%u batt=%umV",
                header.device_id,
                msg_data.v2_0.status_report.index_a,
                msg_data.v2_0.status_report.index_b,
                msg_data.v2_0.status_report.index_c,
                msg_data.v2_0.status_report.battery_voltage);
        if (app_state.config.status_cb) {
            app_state.config.status_cb(header.device_id,
                                       msg_data.v2_0.status_report.index_a,
                                       msg_data.v2_0.status_report.index_b,
                                       msg_data.v2_0.status_report.index_c,
                                       msg_data.v2_0.status_report.battery_voltage);
        }
        break;

    case SLMSGTYPE_DEVICE_INFO:
        LOG_INF("RX DEVICE_INFO from 0x%08X: app=0x%08X model=%u",
                header.device_id,
                msg_data.v2_0.device_info.app_version,
                msg_data.v2_0.device_info.model_id);
        break;

    case SLMSGTYPE_TEST:
        LOG_INF("RX TEST from 0x%08X: len=%u",
                header.device_id,
                msg_data.v2_0.test.payload_len);
        break;

    default:
        LOG_DBG("RX msg_type=%u from 0x%08X", header.message_type, header.device_id);
        break;
    }
}

static int send_message(uint8_t msg_type, void *msg_data)
{
    slmsg_header_t header;

    /* Initialize header */
    slmsg_init_broadcast_header(&app_state.device, msg_type, &header);

    /* Pack message */
    if (!slmsg_pack_msg(&header, msg_data, app_state.device.master_key, &tx_frame)) {
        LOG_ERR("Failed to pack message type %u", msg_type);
        return -EINVAL;
    }

    /* Transmit */
    sramlink_transmit_frame(&tx_frame, TIME_PER_TRANSMITTER);

    /* Wait for completion */
    sramlink_wait_transmit_complete();

    LOG_DBG("TX msg_type=%u len=%u", msg_type, tx_frame.length);
    return 0;
}

/*---------------------------------------------------------------------------
 * Public API
 *---------------------------------------------------------------------------*/

int sramlink_app_init(const sramlink_app_config_t *config)
{
    if (app_state.initialized) {
        LOG_WRN("Already initialized");
        return -EALREADY;
    }

    if (config == NULL) {
        return -EINVAL;
    }

    /* Store configuration */
    app_state.config = *config;

    /* Initialize device subsystem */
    sramlink_device_init();

    /* Load device data from NVS */
    sramlink_device_load(&app_state.device);

    /* Override with shared fixed key so all boards can communicate */
    static const uint8_t shared_key[16] = {
        0x53, 0x52, 0x41, 0x4D, 0x4C, 0x49, 0x4E, 0x4B,  /* "SRAMLINK" */
        0x50, 0x41, 0x49, 0x52, 0x4B, 0x45, 0x59, 0x31   /* "PAIRKEY1" */
    };
    memcpy(app_state.device.master_key, shared_key, 16);
    app_state.device.master_id = 0x534C4E4B;  /* "SLNK" */
    LOG_INF("Using shared master_key, master_id=0x%08X", app_state.device.master_id);

    /* Set device type if specified */
    if (config->device_type != 0) {
        app_state.device.device_type = config->device_type;
    }

    /* Initialize radio */
    sl_radio_init();

    /* Configure channel */
    uint8_t channel = config->radio_channel;
    if (channel < 11 || channel > 26) {
        channel = CONFIG_SRAMLINK_RADIO_CHANNEL;
    }
    sramlink_set_channel(channel);
    app_state.device.radio_channel = channel;

    /* Configure TX power */
    sl_radio_tx_power_t power;
    if (config->tx_power_dbm >= 8) {
        power = TX_POW_POS_8_0_DBM;
    } else if (config->tx_power_dbm >= 4) {
        power = TX_POW_POS_4_0_DBM;
    } else if (config->tx_power_dbm >= 0) {
        power = TX_POW_ZERO_DBM;
    } else {
        power = TX_POW_NEG_4_DBM;
    }
    sramlink_set_tx_power(power);

    /* Configure RX mode */
    if (config->continuous_rx) {
        sramlink_continuous_rx_enable(true);
    }
    sramlink_rx_enable(config->enable_rx);

    /* Set default message version */
    slmsg_set_default_version(SL_VERSION_2_0);

    app_state.initialized = true;
    app_state.running = false;
    app_state.pairing_mode = false;

    LOG_INF("SRAMLink initialized: device_id=0x%08X channel=%u",
            app_state.device.device_id, channel);

    return 0;
}

void sramlink_app_process(void)
{
    if (!app_state.initialized || !app_state.running) {
        return;
    }

    /* Update SRAMLink state machine */
    sramlink_update();

    /* Check for received frames */
    uint8_t lqi;
    while (sramlink_receive_frame(&rx_frame, &lqi)) {
        process_received_message(&rx_frame, lqi);
    }

    /* Pairing mode: send PUBLIC_KEY_SHARE periodically and check timeout */
    if (app_state.pairing_mode) {
        uint32_t now = k_uptime_get_32();

        /* Check 30s timeout */
        if ((now - app_state.pairing_start_time) >= PAIRING_TIMEOUT_MS) {
            LOG_WRN("Pairing timed out after %u ms", PAIRING_TIMEOUT_MS);
            sramlink_app_stop_pairing();
        }
        /* Send PUBLIC_KEY_SHARE every 500ms */
        else if ((now - app_state.pairing_last_tx_time) >= PAIRING_TX_INTERVAL_MS) {
            app_state.pairing_last_tx_time = now;
            sramlink_app_send_public_key_share();
        }
    }

    /* Check if device data needs to be stored */
    if (sramlink_device_needs_store(&app_state.device)) {
        sramlink_device_store(&app_state.device);
    }

    if (sramlink_device_paired_rolling_codes_need_store(&app_state.device)) {
        sramlink_device_store_paired_rolling_codes(&app_state.device);
    }
}

int sramlink_app_start(void)
{
    if (!app_state.initialized) {
        return -EINVAL;
    }

    sramlink_start();

    /* Re-enable RX after sramlink_start() which resets rx_enable/rx_continuous */
    if (app_state.config.continuous_rx) {
        sramlink_continuous_rx_enable(true);
    }
    sramlink_rx_enable(app_state.config.enable_rx);

    app_state.running = true;

    LOG_INF("SRAMLink started (rx=%d, continuous_rx=%d)",
            app_state.config.enable_rx, app_state.config.continuous_rx);
    return 0;
}

void sramlink_app_stop(void)
{
    if (!app_state.running) {
        return;
    }

    sramlink_stop();
    app_state.running = false;

    LOG_INF("SRAMLink stopped");
}

bool sramlink_app_is_running(void)
{
    return app_state.running;
}

int sramlink_app_send_buttons(uint8_t button_mask, uint8_t flags)
{
    if (!app_state.running) {
        return -EINVAL;
    }

    slmsg_v2_0_buttons_t msg = {
        .rolling_code = sramlink_device_get_next_rc(&app_state.device),
        .button_mask = button_mask,
        .flags = flags,
    };

    return send_message(SLMSGTYPE_BUTTONS, &msg);
}

int sramlink_app_send_status_request(uint8_t request_type)
{
    if (!app_state.running) {
        return -EINVAL;
    }

    slmsg_v2_0_status_request_t msg = {
        .rolling_code = sramlink_device_get_next_rc(&app_state.device),
        .request_type = request_type,
    };

    return send_message(SLMSGTYPE_STATUS_REQUEST, &msg);
}

int sramlink_app_send_status_report(uint8_t index_a, uint8_t index_b,
                                    uint8_t index_c, uint16_t battery_mv,
                                    uint8_t battery_status)
{
    if (!app_state.running) {
        return -EINVAL;
    }

    slmsg_v2_0_status_report_t msg = {
        .rolling_code = sramlink_device_get_next_rc(&app_state.device),
        .index_a = index_a,
        .index_b = index_b,
        .index_c = index_c,
        .flags = 0,
        .battery_voltage = battery_mv,
        .battery_status = battery_status,
    };

    return send_message(SLMSGTYPE_STATUS_REPORT, &msg);
}

int sramlink_app_send_test(const uint8_t *payload, uint8_t len)
{
    if (!app_state.running || payload == NULL) {
        return -EINVAL;
    }

    if (len > SL_TEST_PAYLOAD_MAX) {
        len = SL_TEST_PAYLOAD_MAX;
    }

    slmsg_v2_0_test_t msg = {
        .rolling_code = sramlink_device_get_next_rc(&app_state.device),
        .payload_len = len,
    };
    memcpy(msg.payload, payload, len);

    return send_message(SLMSGTYPE_TEST, &msg);
}

int sramlink_app_send_public_key_share(void)
{
    if (!app_state.running) {
        return -EINVAL;
    }

    slmsg_v2_0_public_key_share_t msg;
    memset(&msg, 0, sizeof(msg));

    /* Set operating channel */
    msg.adv_channel = app_state.config.radio_channel;

    /* Set our master_key as the public_key */
    memcpy(msg.public_key, app_state.device.master_key, 16);

    /* Encode our master_id in aes_challenge_response[0..3] (big-endian) */
    msg.aes_challenge_response[0] = (app_state.device.master_id >> 24) & 0xFF;
    msg.aes_challenge_response[1] = (app_state.device.master_id >> 16) & 0xFF;
    msg.aes_challenge_response[2] = (app_state.device.master_id >> 8) & 0xFF;
    msg.aes_challenge_response[3] = (app_state.device.master_id) & 0xFF;

    /* Use global broadcast header (master_id=0 for unencrypted pairing) */
    slmsg_header_t header;
    slmsg_init_global_broadcast_header(&app_state.device,
                                       SLMSGTYPE_PUBLIC_KEY_SHARE, &header);

    /* Pack with NULL key (unencrypted) */
    if (!slmsg_pack_msg(&header, &msg, NULL, &tx_frame)) {
        LOG_ERR("Failed to pack PUBLIC_KEY_SHARE");
        return -EINVAL;
    }

    /* Transmit */
    sramlink_transmit_frame(&tx_frame, TIME_PER_TRANSMITTER);
    sramlink_wait_transmit_complete();

    LOG_DBG("TX PUBLIC_KEY_SHARE: channel=%u device_id=0x%08X",
            msg.adv_channel, app_state.device.device_id);
    return 0;
}

int sramlink_app_set_channel(uint8_t channel)
{
    if (channel < 11 || channel > 26) {
        return -EINVAL;
    }

    sramlink_set_channel(channel);
    app_state.device.radio_channel = channel;

    LOG_INF("Channel set to %u", channel);
    return 0;
}

uint8_t sramlink_app_get_channel(void)
{
    return sramlink_get_channel();
}

int sramlink_app_set_tx_power(int8_t power_dbm)
{
    sl_radio_tx_power_t power;

    if (power_dbm >= 8) {
        power = TX_POW_POS_8_0_DBM;
    } else if (power_dbm >= 4) {
        power = TX_POW_POS_4_0_DBM;
    } else if (power_dbm >= 0) {
        power = TX_POW_ZERO_DBM;
    } else if (power_dbm >= -4) {
        power = TX_POW_NEG_4_DBM;
    } else if (power_dbm >= -8) {
        power = TX_POW_NEG_9_DBM;
    } else if (power_dbm >= -12) {
        power = TX_POW_NEG_12_DBM;
    } else {
        power = TX_POW_NEG_17_DBM;
    }

    sramlink_set_tx_power(power);
    return 0;
}

bool sramlink_app_is_paired(void)
{
    for (int i = 0; i < MAX_PAIRING_RECORDS; i++) {
        if (app_state.device.paired_devices[i].active) {
            return true;
        }
    }
    return false;
}

uint32_t sramlink_app_get_device_id(void)
{
    return app_state.device.device_id;
}

int sramlink_app_start_pairing(void)
{
    if (!app_state.initialized) {
        return -EINVAL;
    }

    if (app_state.pairing_mode) {
        LOG_WRN("Already in pairing mode");
        return 0;
    }

    app_state.pairing_mode = true;
    app_state.pairing_start_time = k_uptime_get_32();
    app_state.pairing_last_tx_time = 0;
    app_state.pairing_complete = false;

    /* Disable RX and wait for any in-progress TX to finish before channel switch */
    sramlink_rx_enable(false);
    sramlink_wait_transmit_complete();
    k_msleep(10);

    /* Switch to pairing channel (26) */
    sramlink_set_channel(PAIRING_SEED_RADIO_CHANNEL);

    /* Enable continuous RX during pairing */
    sramlink_continuous_rx_enable(true);
    sramlink_rx_enable(true);

    LOG_INF("Pairing mode started on channel %u (timeout=%us)",
            PAIRING_SEED_RADIO_CHANNEL, PAIRING_TIMEOUT_MS / 1000);
    return 0;
}

void sramlink_app_stop_pairing(void)
{
    if (!app_state.pairing_mode) {
        return;
    }

    app_state.pairing_mode = false;

    /* Restore normal channel */
    sramlink_set_channel(app_state.device.radio_channel);

    /* Restore RX mode */
    sramlink_continuous_rx_enable(app_state.config.continuous_rx);

    /* Save device data */
    sramlink_device_store(&app_state.device);

    LOG_INF("Pairing mode stopped");
}

int sramlink_app_clear_pairings(void)
{
    erase_all_pairings(&app_state.device);
    sramlink_device_store(&app_state.device);

    LOG_INF("All pairings cleared");
    return 0;
}

bool sramlink_app_pairing_just_completed(void)
{
    if (app_state.pairing_complete) {
        app_state.pairing_complete = false;
        return true;
    }
    return false;
}

uint8_t sramlink_app_get_paired_count(void)
{
    uint8_t count = 0;
    for (int i = 0; i < MAX_PAIRING_RECORDS; i++) {
        if (app_state.device.paired_devices[i].active) {
            count++;
        }
    }
    return count;
}
