/**
 * @file sl_radio_ncs.c
 * @brief SRAMLink Radio Driver for nRF Connect SDK
 *
 * Implements sl_radio.h interface using nrf_802154 driver for
 * nRF52840 and nRF5340 platforms.
 */

#include "sl_radio.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/random/random.h>

#if defined(CONFIG_NRF_802154_RADIO_DRIVER) || defined(CONFIG_NRF_802154_SER_HOST)
#include <nrf_802154.h>
#include <nrf_802154_types.h>
#define SL_RADIO_802154_ENABLED 1
#endif

#if defined(CONFIG_NRF_802154_SER_HOST)
#include <nrf_802154_serialization_error.h>
#endif

#if defined(CONFIG_MBEDTLS)
#include <mbedtls/aes.h>
#endif

LOG_MODULE_REGISTER(sl_radio, CONFIG_LOG_DEFAULT_LEVEL);

/* Radio state */
static struct {
    bool initialized;
    bool awake;
    uint8_t channel;
    sl_radio_tx_power_t tx_power;
    uint8_t data_rate;
    bool crc_error;
    uint8_t last_lqi;
    uint8_t last_ed;
    uint32_t reset_count;
} radio_state = {
    .initialized = false,
    .awake = false,
    .channel = 15,
    .tx_power = SL_RADIO_DEFAULT_TX_POWER,
    .data_rate = SL_RADIO_DEFAULT_DATA_RATE,
    .crc_error = false,
    .last_lqi = 0,
    .last_ed = 0,
    .reset_count = 0,
};

/* RX buffer management */
#define RX_BUFFER_COUNT     4
#define RX_BUFFER_SIZE      (SL_RADIO_FRAME_MAX_LEN + 3)  /* +FCS +LEN */

static uint8_t rx_buffers[RX_BUFFER_COUNT][RX_BUFFER_SIZE];
static volatile int rx_buffer_write_idx = 0;
static volatile int rx_buffer_read_idx = 0;
static volatile bool rx_frame_available = false;

/* AES key storage */
static uint8_t aes_key[16];

#if defined(CONFIG_MBEDTLS)
static mbedtls_aes_context aes_ctx;
#endif

/* TX buffer */
static uint8_t tx_buffer[RX_BUFFER_SIZE];
static volatile bool tx_complete = true;

/* Convert sl_radio tx power to nrf_802154 power */
static int8_t convert_tx_power(sl_radio_tx_power_t power)
{
    /* Map enum values to dBm */
    static const int8_t power_map[] = {
        -17, -12, -9, -7, -5, -4, -3, -2, -1,
        0, 1, 1, 2, 2, 3, 3, 4, 5, 6, 7, 8
    };

    if (power <= TX_POW_POS_8_0_DBM) {
        return power_map[power];
    }
    return 0;
}

bool sl_radio_valid_channel(uint8_t channel_num)
{
    return (channel_num >= SL_RADIO_MIN_CHANNEL &&
            channel_num <= SL_RADIO_MAX_CHANNEL);
}

bool sl_radio_valid_txpower(sl_radio_tx_power_t tx_power)
{
    return (tx_power <= TX_POW_POS_8_0_DBM);
}

void sl_radio_init(void)
{
    if (radio_state.initialized) {
        return;
    }

    LOG_INF("Initializing SRAMLink radio driver");

#if defined(SL_RADIO_802154_ENABLED)
    LOG_INF("Calling nrf_802154_init...");
    nrf_802154_init();
    LOG_INF("nrf_802154_init done");

    /* Set default channel */
    LOG_INF("Setting channel %d", radio_state.channel);
    nrf_802154_channel_set(radio_state.channel);

    /* Set default TX power */
    nrf_802154_tx_power_set(convert_tx_power(radio_state.tx_power));

    /* Configure for promiscuous mode (accept all frames) */
    nrf_802154_promiscuous_set(true);

#if defined(CONFIG_NRF_802154_RADIO_DRIVER)
    /* Configure auto-ack disabled (SRAMLink doesn't use 802.15.4 ACKs) */
    /* Note: Not available on serialized driver */
    nrf_802154_auto_ack_set(false);
#endif

    /* Start receiving */
    LOG_INF("Starting receive mode...");
    if (!nrf_802154_receive()) {
        LOG_ERR("Failed to start radio receive mode");
    }
    LOG_INF("Radio receive mode started");
#endif

#if defined(CONFIG_MBEDTLS)
    mbedtls_aes_init(&aes_ctx);
#endif

    radio_state.initialized = true;
    radio_state.awake = true;

    LOG_INF("Radio initialized on channel %d", radio_state.channel);
}

void sl_radio_reset(void)
{
    LOG_DBG("Radio reset");

#if defined(SL_RADIO_802154_ENABLED)
    nrf_802154_deinit();
#endif

    radio_state.initialized = false;
    radio_state.reset_count++;

    sl_radio_init();
}

void sl_radio_wake(void)
{
    if (radio_state.awake) {
        return;
    }

    LOG_DBG("Radio wake");

#if defined(SL_RADIO_802154_ENABLED)
    if (!nrf_802154_receive()) {
        LOG_ERR("Failed to start receive after wake");
    }
#endif

    radio_state.awake = true;
}

void sl_radio_sleep(void)
{
    if (!radio_state.awake) {
        return;
    }

    LOG_DBG("Radio sleep");

#if defined(SL_RADIO_802154_ENABLED)
    nrf_802154_sleep();
#endif

    radio_state.awake = false;
}

bool sl_radio_is_awake(void)
{
    return radio_state.awake;
}

void sl_radio_set_channel(uint8_t channel_number)
{
    if (!sl_radio_valid_channel(channel_number)) {
        LOG_WRN("Invalid channel %d, ignoring", channel_number);
        return;
    }

    radio_state.channel = channel_number;

#if defined(SL_RADIO_802154_ENABLED)
    nrf_802154_channel_set(channel_number);
#endif

    LOG_DBG("Channel set to %d", channel_number);
}

uint8_t sl_radio_get_channel(void)
{
    return radio_state.channel;
}

void sl_radio_set_tx_power(sl_radio_tx_power_t power)
{
    if (!sl_radio_valid_txpower(power)) {
        LOG_WRN("Invalid TX power %d, ignoring", power);
        return;
    }

    radio_state.tx_power = power;

#if defined(SL_RADIO_802154_ENABLED)
    nrf_802154_tx_power_set(convert_tx_power(power));
#endif

    LOG_DBG("TX power set to level %d", power);
}

void sl_radio_set_rx_sensitivity(sl_radio_rx_sensitivity_t sensitivity)
{
    /* nRF 802154 doesn't support configurable RX sensitivity */
    LOG_DBG("RX sensitivity set request ignored (not supported)");
    (void)sensitivity;
}

void sl_radio_set_data_rate(uint8_t data_rate)
{
    /* nRF 802154 only supports 250kbps */
    if (data_rate != SL_RADIO_DATA_RATE_250K) {
        LOG_WRN("Only 250kbps data rate supported, ignoring");
    }
    radio_state.data_rate = SL_RADIO_DATA_RATE_250K;
}

void sl_radio_set_sfd(uint8_t sfd)
{
    /* nRF 802154 uses standard 802.15.4 SFD */
    LOG_DBG("SFD set request ignored (using standard 802.15.4)");
    (void)sfd;
}

bool sl_radio_tx_frame(sl_radio_frame_t *frame)
{
    if (frame == NULL || frame->length > SL_RADIO_FRAME_MAX_LEN) {
        return false;
    }

#if defined(SL_RADIO_802154_ENABLED)
    /* Format: [length][data...][FCS placeholder] */
    tx_buffer[0] = frame->length + 2;  /* +2 for FCS */
    memcpy(&tx_buffer[1], frame->data, frame->length);

    tx_complete = false;

    /* Transmit (blocking until complete) */
    bool result = nrf_802154_transmit_raw(tx_buffer, NULL);

    if (!result) {
        LOG_ERR("TX failed to start");
        tx_complete = true;
        return false;
    }

    /* Wait for TX complete (with timeout) */
    int timeout = 100;  /* 100ms max */
    while (!tx_complete && timeout > 0) {
        k_msleep(1);
        timeout--;
    }

    if (!tx_complete) {
        LOG_ERR("TX timeout");
        nrf_802154_receive();
        tx_complete = true;
        return false;
    }

    /* Return to RX mode */
    nrf_802154_receive();

    LOG_DBG("TX complete, %d bytes", frame->length);
    return true;
#else
    return false;
#endif
}

bool sl_radio_rx_frame_ready(bool *active)
{
    if (active != NULL) {
        *active = false;
    }

    return rx_frame_available;
}

void sl_radio_rx_frame(sl_radio_frame_t *frame, uint8_t *lqi)
{
    if (!rx_frame_available) {
        if (frame != NULL) {
            frame->length = 0;
        }
        return;
    }

    uint8_t *rx_buf = rx_buffers[rx_buffer_read_idx];
    uint8_t len = rx_buf[0];

    if (frame != NULL) {
        /* Remove FCS from length */
        frame->length = (len > 2) ? (len - 2) : 0;
        if (frame->length > SL_RADIO_FRAME_MAX_LEN) {
            frame->length = SL_RADIO_FRAME_MAX_LEN;
        }
        memcpy(frame->data, &rx_buf[1], frame->length);
    }

    if (lqi != NULL) {
        *lqi = radio_state.last_lqi;
    }

    /* Advance read index (buffer already freed in RX callback) */
    rx_buffer_read_idx = (rx_buffer_read_idx + 1) % RX_BUFFER_COUNT;
    rx_frame_available = (rx_buffer_read_idx != rx_buffer_write_idx);

    LOG_DBG("RX frame retrieved, %d bytes", frame ? frame->length : 0);
}

bool sl_radio_crc_error_detected(void)
{
    return radio_state.crc_error;
}

void sl_radio_clear_crc_error(void)
{
    radio_state.crc_error = false;
}

int8_t sl_radio_get_rssi(void)
{
#if defined(CONFIG_NRF_802154_RADIO_DRIVER)
    /* Direct driver has RSSI measurement */
    return nrf_802154_rssi_last_get();
#else
    /* Serialized driver - return stored value from last RX */
    return -100;
#endif
}

uint8_t sl_radio_get_last_lqi(void)
{
    return radio_state.last_lqi;
}

uint8_t sl_radio_get_last_ed(void)
{
    return radio_state.last_ed;
}

uint8_t sl_radio_perform_ed(void)
{
#if defined(SL_RADIO_802154_ENABLED)
    /* nrf_802154 ED scan is async - for now return last value */
    return radio_state.last_ed;
#else
    return 0;
#endif
}

uint8_t sl_radio_random_bits(void)
{
    return sys_rand32_get() & 0x03;
}

uint8_t sl_radio_random_byte(void)
{
    return (uint8_t)sys_rand32_get();
}

void sl_radio_set_aes_key(uint8_t *key_data)
{
    if (key_data == NULL) {
        return;
    }

    memcpy(aes_key, key_data, 16);

#if defined(CONFIG_MBEDTLS)
    mbedtls_aes_setkey_enc(&aes_ctx, aes_key, 128);
#endif

    LOG_DBG("AES key set");
}

void sl_radio_get_aes_key(uint8_t *key_data)
{
    if (key_data == NULL) {
        return;
    }

    memcpy(key_data, aes_key, 16);
}

void sl_radio_aes_encrypt(uint8_t *input, uint8_t *output, uint8_t direction)
{
    if (input == NULL || output == NULL) {
        return;
    }

#if defined(CONFIG_MBEDTLS)
    if (direction == 0) {
        /* Encrypt */
        mbedtls_aes_setkey_enc(&aes_ctx, aes_key, 128);
        mbedtls_aes_crypt_ecb(&aes_ctx, MBEDTLS_AES_ENCRYPT, input, output);
    } else {
        /* Decrypt */
        mbedtls_aes_setkey_dec(&aes_ctx, aes_key, 128);
        mbedtls_aes_crypt_ecb(&aes_ctx, MBEDTLS_AES_DECRYPT, input, output);
    }
#else
    /* Fallback: just copy data if no crypto available */
    memcpy(output, input, 16);
#endif
}

void sl_radio_start_test_mode(sl_radio_test_mode_t mode, uint8_t channel_num)
{
    LOG_WRN("Test mode not implemented for NCS driver");
    (void)mode;
    (void)channel_num;
}

void sl_radio_stop_test_mode(void)
{
    LOG_WRN("Test mode not implemented for NCS driver");
}

uint8_t sl_radio_get_partnum(void)
{
    /* Return a value indicating nRF platform */
    return 0x52;  /* 'R' for nRF */
}

uint8_t sl_radio_get_part_version(void)
{
    return 0x01;
}

uint8_t sl_radio_get_total_retries(void)
{
    return (uint8_t)(radio_state.reset_count & 0xFF);
}

uint32_t sl_radio_get_reset_count(void)
{
    return radio_state.reset_count;
}

/* nrf_802154 callbacks */
#if defined(SL_RADIO_802154_ENABLED)

static void handle_received_frame(uint8_t *p_data, int8_t power, uint8_t lqi)
{
    /* Store LQI and RSSI */
    radio_state.last_lqi = lqi;
    radio_state.last_ed = (uint8_t)((power + 100) & 0xFF);  /* Convert to ED-like value */

    /* Copy to our buffer and free the 802.15.4 driver buffer */
    int next_idx = (rx_buffer_write_idx + 1) % RX_BUFFER_COUNT;
    if (next_idx != rx_buffer_read_idx) {
        /* Buffer available */
        memcpy(rx_buffers[rx_buffer_write_idx], p_data, p_data[0] + 1);
        rx_buffer_write_idx = next_idx;
        rx_frame_available = true;
    } else {
        /* Buffer full, drop frame */
        LOG_WRN("RX buffer full, dropping frame");
    }

    /* Always free the driver buffer after copying */
    nrf_802154_buffer_free_raw(p_data);
}

#if defined(CONFIG_NRF_802154_SER_HOST)
/* nRF5340: serialization host uses timestamp variant */
void nrf_802154_received_timestamp_raw(uint8_t *p_data, int8_t power, uint8_t lqi, uint64_t time)
{
    (void)time;
    handle_received_frame(p_data, power, lqi);
}
#else
/* nRF52840: direct driver uses non-timestamp variant */
void nrf_802154_received_raw(uint8_t *p_data, int8_t power, uint8_t lqi)
{
    handle_received_frame(p_data, power, lqi);
}
#endif

void nrf_802154_receive_failed(nrf_802154_rx_error_t error, uint32_t id)
{
    (void)id;

    if (error == NRF_802154_RX_ERROR_INVALID_FCS) {
        radio_state.crc_error = true;
        LOG_DBG("RX CRC error");
    } else {
        LOG_WRN("RX failed: error %d", error);
    }
}

void nrf_802154_transmitted_raw(uint8_t *p_frame,
                                const nrf_802154_transmit_done_metadata_t *p_metadata)
{
    (void)p_frame;
    (void)p_metadata;
    tx_complete = true;
}

void nrf_802154_transmit_failed(uint8_t *p_frame,
                                nrf_802154_tx_error_t error,
                                const nrf_802154_transmit_done_metadata_t *p_metadata)
{
    (void)p_frame;
    (void)p_metadata;

    LOG_ERR("TX failed: error %d", error);
    tx_complete = true;
}

void nrf_802154_energy_detected(const nrf_802154_energy_detected_t *p_result)
{
    radio_state.last_ed = p_result->ed_dbm + 100;  /* Convert to 0-based */
}

#if defined(CONFIG_NRF_802154_SER_HOST)
/* Serialization error callback - required for nRF5340 app core */
void nrf_802154_serialization_error(const nrf_802154_ser_err_data_t *err)
{
    LOG_ERR("802.15.4 serialization error: %d", err ? err->reason : -1);
}
#endif /* CONFIG_NRF_802154_SER_HOST */

#endif /* SL_RADIO_802154_ENABLED */
