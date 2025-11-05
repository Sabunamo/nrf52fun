#ifndef BME280_SENSOR_H
#define BME280_SENSOR_H

#include <zephyr/kernel.h>
#include <stdbool.h>

/**
 * @brief BME280 sensor data structure
 */
typedef struct {
    float temperature;  // Temperature in Celsius
    float humidity;     // Relative humidity in %
    float pressure;     // Pressure in hPa
    bool valid;         // Data validity flag
} bme280_data_t;

/**
 * @brief Initialize BME280 sensor
 * @return 0 on success, negative error code on failure
 */
int bme280_sensor_init(void);

/**
 * @brief Read all sensor data from BME280
 * @param data Pointer to structure to store sensor readings
 * @return 0 on success, negative error code on failure
 */
int bme280_sensor_read(bme280_data_t *data);

/**
 * @brief Get last valid sensor reading
 * @param data Pointer to structure to store sensor readings
 * @return 0 on success, negative error code if no valid data
 */
int bme280_sensor_get_data(bme280_data_t *data);

/**
 * @brief Check if BME280 sensor is available
 * @return true if sensor is ready, false otherwise
 */
bool bme280_sensor_is_ready(void);

#endif // BME280_SENSOR_H
