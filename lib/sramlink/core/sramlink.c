/**
 * @file sramlink.c
 * @brief SRAMLink Engine implementation for Zephyr
 *
 * Ported from bambam SRAMLink for nRF Connect SDK.
 */

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "../drivers/sys_time_zephyr.h"
#include "../drivers/sl_radio.h"
#include "../drivers/cdtimer_zephyr.h"
#include "../messages/sramlink_messages.h"
#include "sramlink.h"
#include "sramlink_event.h"

LOG_MODULE_REGISTER(sramlink, CONFIG_LOG_DEFAULT_LEVEL);

typedef void SRAMLINK_STATE(void);

/****************** STATEMACHINE VARIABLES *********************/
static SRAMLINK_STATE *sramlink_current_state;
static uint8_t sramlink_current_sub_state;
static sys_time_t state_machine_time;

/****************** RX ON/OFF VARIABLES *********************/
static sys_time_t radio_wake_up_time;
static bool rx_enable;
static bool rx_continuous;
static bool radio_requested;
static sys_time_t radio_timer;
static sys_time_t radio_update_time;
static uint32_t radio_wake_time;
static cdtimer_t radio_stolen_timer;
static bool m_sramlink_radio_stolen_long_rx;

/****************** PACKET VARIABLES *********************/
static sl_radio_frame_t radio_rx_frame;
static sl_radio_frame_t radio_tx_frame;
static uint16_t max_tx_packet_count;
static uint8_t tx_packet_count;
static uint32_t tx_packet_count_cumulative;
static bool radio_frame_received;

/************************* TX VARIABLES *****************************/
static sl_priority_t tx_priority;
static sys_time_t transmit_base_time;
static uint8_t hash_bit_mask[256 / 8];
static uint8_t other_transmitter_count;
static uint8_t max_other_transmitter_count;
static uint32_t added_jitter;

/************************* ED VARIABLES *****************************/
static uint8_t radio_rx_lqi;
static int8_t rssi_threshold = DEFAULT_RSSI_THRESHOLD;
static bool ed_latched;

static sl_evt_cb_t evt_cb;
static void *evt_data;
static sys_time_t m_cached_radio_timer;

/* Forward declarations */
static void receive_state(void);
static void transmit_state(void);
static void transmitter_initial_look_state(void);
static void force_idle_state(void);

void sramlink_radio_lost(void)
{
    cdtimer_restart(&radio_stolen_timer);
    if (radio_update_time == RX_LONG_ON_INTERVAL) {
        m_cached_radio_timer = radio_timer;
        m_sramlink_radio_stolen_long_rx = true;
    }
}

void sramlink_radio_acquired(void)
{
    if (m_sramlink_radio_stolen_long_rx) {
        if (!cdtimer_completed(&radio_stolen_timer)) {
            radio_timer = m_cached_radio_timer +
                          SYS_MS(cdtimer_elapsed(&radio_stolen_timer));
        } else {
            radio_timer += SYS_MS(RADIO_LOST_MAX_TIME_MS);
        }
        m_sramlink_radio_stolen_long_rx = false;
    }
}

static bool set_hash_bit_mask(uint8_t hash)
{
    bool result;
    result = (hash_bit_mask[hash / 8] & (1 << (hash & 7))) != 0;
    hash_bit_mask[hash / 8] |= (1 << (hash & 7));
    return result;
}

static void clear_hash_bit_mask(void)
{
    memset(hash_bit_mask, 0x00, sizeof(hash_bit_mask));
}

static uint8_t device_id_hash(uint32_t device_id)
{
    uint8_t hash = 0;
    hash ^= (device_id >> 24) & 0xFF;
    hash ^= (device_id >> 16) & 0xFF;
    hash ^= (device_id >> 8) & 0xFF;
    hash ^= (device_id >> 0) & 0xFF;
    return hash;
}

static void set_radio_requested(bool requested)
{
    if (radio_requested != requested) {
        if (requested) {
            sl_radio_wake();
            radio_wake_up_time = state_machine_time;
        } else {
            sl_radio_sleep();
            radio_wake_time += (uint32_t)(state_machine_time - radio_wake_up_time);
        }
        radio_requested = requested;
    }
}

static void restore_radio_sleep_state(void)
{
    if (!radio_requested) {
        sl_radio_sleep();
    }
}

static void force_radio_requested(void)
{
    if (!radio_requested) {
        sl_radio_wake();
    }
}

static void call_cb(sl_evt_t event)
{
    if (evt_cb != NULL) {
        evt_cb(event, tx_packet_count, evt_data);
    }
}

static void set_state(SRAMLINK_STATE *new_state)
{
    sramlink_current_state = new_state;
    sramlink_current_sub_state = 0;
}

static void force_idle_state(void)
{
    bool rxActive;

    if (sramlink_current_sub_state == 0) {
        set_radio_requested(true);
        radio_timer = state_machine_time;
        radio_update_time = RX_ON_INTERVAL;
        radio_frame_received = false;
        sramlink_current_sub_state = 1;
    }

    if (!radio_requested) {
        if ((sys_time_t)(state_machine_time - radio_timer) >= radio_update_time) {
            set_radio_requested(true);
            radio_timer = state_machine_time;
            radio_update_time = RX_ON_INTERVAL;
        }
    }

    if (sl_radio_is_awake()) {
        if (sl_radio_rx_frame_ready(&rxActive)) {
            sl_radio_rx_frame(&radio_rx_frame, &radio_rx_lqi);
        }
        if ((sys_time_t)(state_machine_time - radio_timer) >= radio_update_time) {
            set_radio_requested(false);
            radio_timer = state_machine_time;
            radio_update_time = RX_OFF_INTERVAL;
        }
    }
}

static void receive_state(void)
{
    bool rxActive;

    if (sramlink_current_sub_state == 0) {
        if (rx_enable) {
            set_radio_requested(true);
            radio_timer = state_machine_time;
            radio_update_time = RX_LONG_ON_INTERVAL;
        } else {
            set_radio_requested(false);
        }
        ed_latched = false;
        sramlink_current_sub_state = 1;
    }

    if ((!radio_requested && rx_enable) &&
        (((sys_time_t)(state_machine_time - radio_timer) >= radio_update_time) ||
         sl_radio_is_awake())) {
        set_radio_requested(true);
        radio_timer = state_machine_time;
        radio_update_time = RX_ON_INTERVAL;
    }

    if (sl_radio_is_awake()) {
        if (sl_radio_rx_frame_ready(&rxActive)) {
            sl_radio_rx_frame(&radio_rx_frame, &radio_rx_lqi);
            radio_timer = state_machine_time;
            radio_update_time = RX_LONG_ON_INTERVAL;
            radio_frame_received = (radio_rx_frame.length >= SRAMLINK_HEADER_LENGTH);
        } else if (sl_radio_crc_error_detected()) {
            sl_radio_clear_crc_error();
            LOG_DBG("RX CRC error");
            radio_timer = state_machine_time;
            radio_update_time = RX_LONG_ON_INTERVAL;
        } else if (!rxActive && !rx_continuous) {
            if ((rssi_threshold != 0) && (sl_radio_get_rssi() >= rssi_threshold)) {
                radio_timer = state_machine_time;
                radio_update_time = RSSI_HOLD_TIME;
                ed_latched = true;
            } else if ((sys_time_t)(state_machine_time - radio_timer) >= radio_update_time) {
                set_radio_requested(false);
                ed_latched = false;
                radio_timer = state_machine_time;
                radio_update_time = RX_OFF_INTERVAL;
            }
        }
    }
}

static sys_time_t get_random_jitter_time(void)
{
    uint8_t rand = sl_radio_random_bits();
    if (rand & 1) {
        added_jitter++;
        return SYS_MS(1);
    }
    return 0;
}

static void transmit_state(void)
{
    bool have_frame;
    bool getting_frame;
    slmsg_header_t header;
    static sys_time_t additional_delay;

    if (sramlink_current_sub_state == 0) {
        additional_delay = get_random_jitter_time();
        sramlink_current_sub_state = 1;
    }

    have_frame = sl_radio_rx_frame_ready(&getting_frame);
    if (have_frame) {
        sl_radio_rx_frame(&radio_rx_frame, &radio_rx_lqi);

        if (radio_rx_frame.length >= SRAMLINK_HEADER_LENGTH) {
            slmsg_unpack_header(&radio_rx_frame, &header);
            radio_frame_received = rx_enable;

            if (!set_hash_bit_mask(device_id_hash(header.device_id))) {
                other_transmitter_count++;
                if (other_transmitter_count > max_other_transmitter_count) {
                    max_other_transmitter_count = other_transmitter_count;
                }
                radio_update_time = (1 + other_transmitter_count) * TIME_PER_TRANSMITTER;
            }
        } else {
            radio_frame_received = false;
        }
    }

    if ((sys_time_t)(state_machine_time - radio_timer) >=
        radio_update_time + additional_delay +
        (getting_frame ? TIME_PER_TRANSMITTER : 0)) {

        if (sl_radio_tx_frame(&radio_tx_frame)) {
            tx_packet_count++;
            tx_packet_count_cumulative++;
        }

        if ((tx_packet_count < max_tx_packet_count) &&
            ((sys_time_t)(state_machine_time - transmit_base_time) < MAX_TRANSMIT_TIME)) {
            uint32_t previous_delay = additional_delay;
            additional_delay = get_random_jitter_time();
            radio_timer = state_machine_time - previous_delay;
            radio_update_time = (1 + other_transmitter_count) * TIME_PER_TRANSMITTER;
        } else {
            call_cb(SL_EVT_TX_COMPLETE);
            set_state(receive_state);
        }
    }
}

static void transmitter_initial_look_state(void)
{
    bool have_frame;
    bool getting_frame;
    bool jump_now;
    slmsg_header_t header;

    if (sramlink_current_sub_state == 0) {
        set_radio_requested(true);
        other_transmitter_count = 0;
        radio_timer = state_machine_time;
        clear_hash_bit_mask();
        sramlink_current_sub_state = 1;
    }

    jump_now = false;
    have_frame = sl_radio_rx_frame_ready(&getting_frame);

    if (have_frame) {
        sl_radio_rx_frame(&radio_rx_frame, &radio_rx_lqi);

        if (radio_rx_frame.length >= SRAMLINK_HEADER_LENGTH) {
            slmsg_unpack_header(&radio_rx_frame, &header);
            radio_frame_received = rx_enable;

            if (!set_hash_bit_mask(device_id_hash(header.device_id))) {
                other_transmitter_count++;
                if (other_transmitter_count > max_other_transmitter_count) {
                    max_other_transmitter_count = other_transmitter_count;
                }
                radio_timer = state_machine_time;
            } else {
                jump_now = true;
            }
        } else {
            radio_frame_received = false;
        }
    }

    if (jump_now ||
        ((sys_time_t)(state_machine_time - radio_timer) >=
         QUIET_TIME_BEFORE_TX + (getting_frame ? TIME_PER_TRANSMITTER : 0))) {

        if (sl_radio_tx_frame(&radio_tx_frame)) {
            tx_packet_count++;
            tx_packet_count_cumulative++;
        }

        if (tx_packet_count < max_tx_packet_count) {
            uint32_t update_interval = (1 + other_transmitter_count) * TIME_PER_TRANSMITTER +
                                       (jump_now ? (TIME_PER_TRANSMITTER / 2) : 0);
            radio_timer = state_machine_time;
            radio_update_time = update_interval;
            set_state(transmit_state);
        } else {
            call_cb(SL_EVT_TX_COMPLETE);
            set_state(receive_state);
        }
    }
}

static void aes_buffer(uint8_t *buffer, uint8_t buffer_length, uint8_t direction)
{
    uint8_t i;
    uint8_t unused[16];

    sl_radio_aes_encrypt(&buffer[0], unused, direction);
    for (i = 0; i < buffer_length - 16; i += 16) {
        sl_radio_aes_encrypt(&buffer[i + 16], &buffer[i], direction);
    }
    sl_radio_aes_encrypt(unused, &buffer[i], direction);
}

void sramlink_aes_decrypt(uint8_t *key, uint8_t *buffer, uint8_t buffer_length)
{
    uint8_t temp[16];

    force_radio_requested();
    sl_radio_set_aes_key(key);
    sl_radio_aes_encrypt(&temp[0], &temp[0], 0);
    sl_radio_get_aes_key(&temp[0]);
    sl_radio_set_aes_key(&temp[0]);
    aes_buffer(buffer, buffer_length, 1);
    restore_radio_sleep_state();
}

void sramlink_aes_encrypt(uint8_t *key, uint8_t *buffer, uint8_t buffer_length)
{
    force_radio_requested();
    sl_radio_set_aes_key(key);
    aes_buffer(buffer, buffer_length, 0);
    restore_radio_sleep_state();
}

void sramlink_enable_force_idle(void)
{
    set_state(force_idle_state);
}

bool sramlink_transmitter_busy(void)
{
    return (sramlink_current_state != receive_state);
}

sl_priority_t sramlink_current_tx_priority(void)
{
    if (!sramlink_transmitter_busy()) {
        return SL_PRIORITY_INVALID;
    }
    return tx_priority;
}

bool sramlink_busy(void)
{
    return (radio_requested || sramlink_transmitter_busy());
}

uint8_t sramlink_get_packet_count(void)
{
    return tx_packet_count;
}

bool sramlink_receive_frame(sl_radio_frame_t *frame, uint8_t *lqi)
{
    if (radio_frame_received) {
        *frame = radio_rx_frame;
        if (lqi) {
            *lqi = radio_rx_lqi;
        }
        radio_frame_received = false;
        return true;
    }
    return false;
}

static void transmit_frame_internal(sl_radio_frame_t *frame, sys_time_t single_transmitter_time)
{
    if (frame != NULL) {
        radio_tx_frame = *frame;
    }

    max_tx_packet_count = single_transmitter_time / TIME_PER_TRANSMITTER;
    if (!max_tx_packet_count) {
        max_tx_packet_count = 1;
    }

    tx_packet_count = 0;

    if (!sramlink_transmitter_busy()) {
        set_state(transmitter_initial_look_state);
    }

    transmit_base_time = sys_time_get_awake();
}

bool sramlink_transmit_frame_with_priority(
    sl_radio_frame_t *frame,
    sys_time_t single_transmitter_time,
    sl_priority_t priority,
    sl_evt_cb_t new_evt_cb, void *data)
{
    if (!sramlink_transmitter_busy()) {
        tx_priority = priority;
        transmit_frame_internal(frame, single_transmitter_time);
        evt_cb = new_evt_cb;
        evt_data = data;
        return true;
    }

    if (priority >= tx_priority) {
        if (priority == tx_priority) {
            call_cb(SL_EVT_TX_CLOBBERED);
        } else {
            call_cb(SL_EVT_TX_HIGHER_PRIORITY);
        }
        tx_priority = priority;
        transmit_frame_internal(frame, single_transmitter_time);
        evt_cb = new_evt_cb;
        evt_data = data;
        return true;
    }

    return false;
}

void sramlink_transmit_frame_with_cb(
    sl_radio_frame_t *frame,
    sys_time_t single_transmitter_time,
    sl_evt_cb_t new_evt_cb)
{
    sramlink_transmit_frame_with_priority(frame, single_transmitter_time,
                                          SL_PRIORITY_MAX, new_evt_cb, NULL);
}

void sramlink_transmit_frame(sl_radio_frame_t *frame, sys_time_t single_transmitter_time)
{
    sramlink_transmit_frame_with_priority(frame, single_transmitter_time,
                                          SL_PRIORITY_MAX, NULL, NULL);
}

void sramlink_wait_transmit_complete(void)
{
    if (!radio_requested) {
        return;
    }

    while (sramlink_transmitter_busy()) {
        sramlink_update();
        k_yield();
    }
}

void sramlink_continuous_rx_enable(bool enable)
{
    if (rx_continuous != enable) {
        rx_continuous = enable;
        if (!sramlink_transmitter_busy()) {
            set_state(receive_state);
        }
    }
}

bool sramlink_continuous_rx_is_enabled(void)
{
    return rx_continuous;
}

void sramlink_rx_enable(bool enable)
{
    if (rx_enable != enable) {
        rx_enable = enable;
        if (!sramlink_transmitter_busy()) {
            set_state(receive_state);
        }
    }
}

bool sramlink_rx_is_enabled(void)
{
    return rx_enable;
}

void sramlink_set_rx_sensitivity(sl_radio_rx_sensitivity_t sensitivity)
{
    force_radio_requested();
    sl_radio_set_rx_sensitivity(sensitivity);
    restore_radio_sleep_state();
}

void sramlink_set_tx_power(sl_radio_tx_power_t power)
{
    force_radio_requested();
    sl_radio_set_tx_power(power);
    restore_radio_sleep_state();
}

void sramlink_set_sfd(uint8_t sfd)
{
    force_radio_requested();
    sl_radio_set_sfd(sfd);
    restore_radio_sleep_state();
    radio_frame_received = false;
    set_state(receive_state);
}

bool sramlink_valid_channel(uint8_t channel_number)
{
    return sl_radio_valid_channel(channel_number);
}

void sramlink_set_channel(uint8_t channel_number)
{
    force_radio_requested();
    sl_radio_set_channel(channel_number);
    restore_radio_sleep_state();
    radio_frame_received = false;
    set_state(receive_state);
}

uint8_t sramlink_get_channel(void)
{
    uint8_t radioChannel;
    force_radio_requested();
    radioChannel = sl_radio_get_channel();
    restore_radio_sleep_state();
    return radioChannel;
}

void sramlink_set_data_rate(uint8_t data_rate)
{
    force_radio_requested();
    sl_radio_set_data_rate(data_rate);
    restore_radio_sleep_state();
    radio_frame_received = false;
    set_state(receive_state);
}

void sramlink_set_rssi_threshold(int8_t new_rssi_threshold)
{
    rssi_threshold = new_rssi_threshold;
}

bool sramlink_radio_is_awake(void)
{
    return radio_requested;
}

void sramlink_update(void)
{
    state_machine_time = sys_time_get_awake();
    if (sramlink_current_state != NULL) {
        sramlink_current_state();
    }
}

void sramlink_stop(void)
{
    set_radio_requested(false);
}

void sramlink_start(void)
{
    sl_radio_sleep();
    rx_enable = false;
    rx_continuous = false;
    radio_frame_received = false;
    radio_requested = false;
    m_sramlink_radio_stolen_long_rx = false;
    tx_packet_count = 0;

    cdtimer_init(&radio_stolen_timer, RADIO_LOST_MAX_TIME_MS);
    set_state(receive_state);

    LOG_INF("SRAMLink engine started");
}

void sramlink_reset_cumulative_packet_count(void)
{
    tx_packet_count_cumulative = 0;
}

uint32_t sramlink_get_cumulative_packet_count(void)
{
    return tx_packet_count_cumulative;
}

uint8_t sramlink_get_other_count(void)
{
    return other_transmitter_count;
}

uint32_t sramlink_get_radio_wake_time(void)
{
    return radio_wake_time;
}

void sramlink_reset_radio_wake_time(void)
{
    radio_wake_time = 0;
}

uint8_t sramlink_get_max_other_count(void)
{
    return max_other_transmitter_count;
}

void sramlink_reset_max_other_count(void)
{
    max_other_transmitter_count = 0;
}

uint32_t sramlink_get_added_jitter(void)
{
    return added_jitter;
}

void sramlink_reset_added_jitter(void)
{
    added_jitter = 0;
}

bool sramlink_is_ed_latched(void)
{
    return ed_latched;
}
