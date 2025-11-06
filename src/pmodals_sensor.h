#ifndef PMODALS_SENSOR_H
#define PMODALS_SENSOR_H

#include <zephyr/kernel.h>
#include <stdbool.h>

/**
 * @brief PmodALS (Ambient Light Sensor) data structure
 *
 * The PmodALS uses an ADC081S021 8-bit ADC with SPI interface
 * and an analog ambient light sensor (Vishay TEMT6000)
 */
typedef struct {
    uint8_t raw_value;      // Raw ADC value (0-255)
    uint16_t lux;           // Calculated lux value (approximate)
    uint8_t brightness_pct; // Suggested brightness percentage (0-100)
    bool valid;             // Data validity flag
} pmodals_data_t;

/**
 * @brief Initialize PmodALS sensor
 * @return 0 on success, negative error code on failure
 */
int pmodals_init(void);

/**
 * @brief Read ambient light sensor data
 * @param data Pointer to structure to store sensor readings
 * @return 0 on success, negative error code on failure
 */
int pmodals_read(pmodals_data_t *data);

/**
 * @brief Get last valid sensor reading
 * @param data Pointer to structure to store sensor readings
 * @return 0 on success, negative error code if no valid data
 */
int pmodals_get_data(pmodals_data_t *data);

/**
 * @brief Check if PmodALS sensor is available
 * @return true if sensor is ready, false otherwise
 */
bool pmodals_is_ready(void);

/**
 * @brief Calculate brightness percentage from lux value
 * @param lux Lux value from sensor
 * @return Suggested brightness percentage (0-100)
 */
uint8_t pmodals_lux_to_brightness(uint16_t lux);

#endif // PMODALS_SENSOR_H
