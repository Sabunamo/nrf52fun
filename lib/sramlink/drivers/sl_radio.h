/**
 * @file sl_radio.h
 * @brief SRAMLink Radio Driver Interface
 *
 * Software driver interface for controlling a radio used for SRAMLink.
 * This header defines the API that must be implemented by platform-specific
 * radio drivers.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Define the max and minimum channel numbers (802.15.4)
 */
#define SL_RADIO_MIN_CHANNEL    11      /**< minimum radio channel */
#define SL_RADIO_MAX_CHANNEL    26      /**< maximum radio channel */
#define SL_RADIO_NUM_CHANNELS   (SL_RADIO_MAX_CHANNEL - SL_RADIO_MIN_CHANNEL + 1)

/**
 * @brief Define the data rate
 */
#define SL_RADIO_DATA_RATE_250K     0
#define SL_RADIO_DATA_RATE_500K     1
#define SL_RADIO_DATA_RATE_1000K    2
#define SL_RADIO_DATA_RATE_2000K    3
#define SL_RADIO_DEFAULT_DATA_RATE  SL_RADIO_DATA_RATE_250K

/**
 * @brief Define the CCA mode
 */
#define SL_RADIO_CCA_MODE_CARRIER_OR_ENERGY     0
#define SL_RADIO_CCA_MODE_ENERGY                1
#define SL_RADIO_CCA_MODE_CARRIER               2
#define SL_RADIO_CCA_MODE_CARRIER_AND_ENERGY    3

/**
 * @brief Define the SFD (start of frame delimiter) values
 */
#define SL_RADIO_SFD_DEFAULT    0xE7
#define SL_RADIO_SFD_802_15_4   0xA7

/**
 * @brief Define the maximum frame length
 */
#define SL_RADIO_FRAME_MAX_LEN  125

/**
 * @brief Error return codes
 */
#define SL_RADIO_ERROR_TRX_TIMEOUT 1

/**
 * @brief Radio RX on time for duty cycling (milliseconds)
 */
#define SRAMLINK_RADIO_ON_TIME_MS   5

/**
 * @brief Data sent/received over the air
 */
typedef struct {
    uint8_t length;                         /**< Number of bytes of data (0 to 125) */
    uint8_t data[SL_RADIO_FRAME_MAX_LEN];   /**< Radio data buffer */
} sl_radio_frame_t;

/**
 * @brief TX Power levels
 */
typedef enum {
    TX_POW_NEG_17_DBM  = 0x00,  /**< -17 dBm */
    TX_POW_NEG_12_DBM  = 0x01,  /**< -12 dBm */
    TX_POW_NEG_9_DBM   = 0x02,  /**< -9 dBm */
    TX_POW_NEG_7_DBM   = 0x03,  /**< -7 dBm */
    TX_POW_NEG_5_DBM   = 0x04,  /**< -5 dBm */
    TX_POW_NEG_4_DBM   = 0x05,  /**< -4 dBm */
    TX_POW_NEG_3_DBM   = 0x06,  /**< -3 dBm */
    TX_POW_NEG_2_DBM   = 0x07,  /**< -2 dBm */
    TX_POW_NEG_1_DBM   = 0x08,  /**< -1 dBm */
    TX_POW_ZERO_DBM    = 0x09,  /**< 0.0 dBm */
    TX_POW_POS_0_7_DBM = 0x0A,  /**< 0.7 dBm */
    TX_POW_POS_1_3_DBM = 0x0B,  /**< 1.3 dBm */
    TX_POW_POS_1_8_DBM = 0x0C,  /**< 1.8 dBm */
    TX_POW_POS_2_3_DBM = 0x0D,  /**< 2.3 dBm */
    TX_POW_POS_2_8_DBM = 0x0E,  /**< 2.8 dBm */
    TX_POW_POS_3_0_DBM = 0x0F,  /**< 3.0 dBm */
    TX_POW_POS_4_0_DBM = 0x10,  /**< 4.0 dBm */
    TX_POW_POS_5_0_DBM = 0x11,  /**< 5.0 dBm */
    TX_POW_POS_6_0_DBM = 0x12,  /**< 6.0 dBm */
    TX_POW_POS_7_0_DBM = 0x13,  /**< 7.0 dBm */
    TX_POW_POS_8_0_DBM = 0x14,  /**< 8.0 dBm */
} sl_radio_tx_power_t;

#define SL_RADIO_DEFAULT_TX_POWER   TX_POW_POS_4_0_DBM

/**
 * @brief RX sensitivity levels
 */
typedef enum {
    RX_SENS_MAX        = 0x00,
    RX_SENS_NEG_90_DBM = 0x01,
    RX_SENS_NEG_87_DBM = 0x02,
    RX_SENS_NEG_84_DBM = 0x03,
    RX_SENS_NEG_81_DBM = 0x04,
    RX_SENS_NEG_78_DBM = 0x05,
    RX_SENS_NEG_75_DBM = 0x06,
    RX_SENS_NEG_72_DBM = 0x07,
    RX_SENS_NEG_69_DBM = 0x08,
    RX_SENS_NEG_66_DBM = 0x09,
    RX_SENS_NEG_63_DBM = 0x0A,
    RX_SENS_NEG_60_DBM = 0x0B,
    RX_SENS_NEG_57_DBM = 0x0C,
    RX_SENS_NEG_54_DBM = 0x0D,
    RX_SENS_NEG_51_DBM = 0x0E,
    RX_SENS_NEG_48_DBM = 0x0F,
} sl_radio_rx_sensitivity_t;

#define SL_RADIO_DEFAULT_RX_SENSITIVITY     RX_SENS_MAX

/**
 * @brief CW Test modes
 */
typedef enum {
    SL_RADIO_TEST_CW_LOW  = 0x01, /**< CW mode at Fc-0.5MHz */
    SL_RADIO_TEST_CW_HIGH = 0x02, /**< CW mode at Fc+0.5MHz */
    SL_RADIO_TEST_PRBS    = 0x03  /**< PRBS Mode */
} sl_radio_test_mode_t;

/**
 * @brief Check if a channel number is valid
 * @param channel_num Channel number to validate
 * @return true if valid, false otherwise
 */
bool sl_radio_valid_channel(uint8_t channel_num);

/**
 * @brief Check if tx power level is valid
 * @param tx_power Power level to validate
 * @return true if valid, false otherwise
 */
bool sl_radio_valid_txpower(sl_radio_tx_power_t tx_power);

/**
 * @brief Initialize the radio
 */
void sl_radio_init(void);

/**
 * @brief Reset the radio to default settings
 */
void sl_radio_reset(void);

/**
 * @brief Wake up the radio
 */
void sl_radio_wake(void);

/**
 * @brief Put radio to sleep
 */
void sl_radio_sleep(void);

/**
 * @brief Check if radio is awake
 * @return true if awake, false otherwise
 */
bool sl_radio_is_awake(void);

/**
 * @brief Set the channel number
 * @param channel_number Channel number (11-26)
 */
void sl_radio_set_channel(uint8_t channel_number);

/**
 * @brief Get the current channel
 * @return Current channel number
 */
uint8_t sl_radio_get_channel(void);

/**
 * @brief Set the transmit power
 * @param power Transmit power value
 */
void sl_radio_set_tx_power(sl_radio_tx_power_t power);

/**
 * @brief Set the receive sensitivity
 * @param sensitivity Sensitivity value
 */
void sl_radio_set_rx_sensitivity(sl_radio_rx_sensitivity_t sensitivity);

/**
 * @brief Set the data rate
 * @param data_rate Data rate value
 */
void sl_radio_set_data_rate(uint8_t data_rate);

/**
 * @brief Set the Start of Frame Delimiter
 * @param sfd SFD value
 */
void sl_radio_set_sfd(uint8_t sfd);

/**
 * @brief Transmit a frame
 * @param frame Pointer to frame data
 * @return true if transmission started successfully
 */
bool sl_radio_tx_frame(sl_radio_frame_t *frame);

/**
 * @brief Check if a valid frame has been received
 * @param active Set to true if packet reception is in progress
 * @return true if frame is ready
 */
bool sl_radio_rx_frame_ready(bool *active);

/**
 * @brief Collect the received frame
 * @param frame Pointer to store received data (NULL to discard)
 * @param lqi Pointer to store LQI value (NULL to discard)
 */
void sl_radio_rx_frame(sl_radio_frame_t *frame, uint8_t *lqi);

/**
 * @brief Check if CRC error was detected
 * @return true if CRC error occurred
 */
bool sl_radio_crc_error_detected(void);

/**
 * @brief Clear the CRC error flag
 */
void sl_radio_clear_crc_error(void);

/**
 * @brief Get RSSI in dBm
 * @return RSSI value
 */
int8_t sl_radio_get_rssi(void);

/**
 * @brief Get LQI of last received packet
 * @return LQI value
 */
uint8_t sl_radio_get_last_lqi(void);

/**
 * @brief Get ED value of last received frame
 * @return ED value
 */
uint8_t sl_radio_get_last_ed(void);

/**
 * @brief Perform energy detection
 * @return Energy detect result (0-84)
 */
uint8_t sl_radio_perform_ed(void);

/**
 * @brief Generate random bits from radio
 * @return 2 random bits
 */
uint8_t sl_radio_random_bits(void);

/**
 * @brief Generate random byte from radio
 * @return Random byte
 */
uint8_t sl_radio_random_byte(void);

/**
 * @brief Set AES key
 * @param key_data Pointer to 16-byte key
 */
void sl_radio_set_aes_key(uint8_t *key_data);

/**
 * @brief Get AES key
 * @param key_data Pointer to store 16-byte key
 */
void sl_radio_get_aes_key(uint8_t *key_data);

/**
 * @brief AES encrypt/decrypt
 * @param input Input data (16 bytes)
 * @param output Output data (16 bytes)
 * @param direction 0=encrypt, 1=decrypt
 */
void sl_radio_aes_encrypt(uint8_t *input, uint8_t *output, uint8_t direction);

/**
 * @brief Start test mode
 * @param mode Test mode type
 * @param channel_num Channel for test
 */
void sl_radio_start_test_mode(sl_radio_test_mode_t mode, uint8_t channel_num);

/**
 * @brief Stop test mode
 */
void sl_radio_stop_test_mode(void);

/**
 * @brief Get radio part number
 * @return Part number
 */
uint8_t sl_radio_get_partnum(void);

/**
 * @brief Get radio version
 * @return Version number
 */
uint8_t sl_radio_get_part_version(void);

/**
 * @brief Get total radio resets this power cycle
 * @return Reset count
 */
uint8_t sl_radio_get_total_retries(void);

/**
 * @brief Get reset count since startup
 * @return Reset count
 */
uint32_t sl_radio_get_reset_count(void);

/**
 * @brief Macro to check if radio channel is clear
 */
#define SRAMLINK_RADIO_CLEAR()  (true)
