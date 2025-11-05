#include "bme280_sensor.h"
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(bme280_sensor, LOG_LEVEL_INF);

static const struct device *bme280_dev = NULL;
static bme280_data_t last_reading = {0};

int bme280_sensor_init(void)
{
    // Get BME280 device from devicetree
    bme280_dev = DEVICE_DT_GET_ANY(bosch_bme280);

    if (bme280_dev == NULL) {
        LOG_ERR("BME280 device not found in devicetree");
        return -ENODEV;
    }

    if (!device_is_ready(bme280_dev)) {
        LOG_ERR("BME280 device not ready");
        bme280_dev = NULL;
        return -ENODEV;
    }

    LOG_INF("BME280 sensor initialized successfully");

    // Initialize last_reading
    last_reading.valid = false;
    last_reading.temperature = 0.0f;
    last_reading.humidity = 0.0f;
    last_reading.pressure = 0.0f;

    return 0;
}

int bme280_sensor_read(bme280_data_t *data)
{
    if (data == NULL) {
        return -EINVAL;
    }

    if (bme280_dev == NULL || !device_is_ready(bme280_dev)) {
        LOG_ERR("BME280 device not ready for reading");
        data->valid = false;
        return -ENODEV;
    }

    // Fetch sensor data
    int ret = sensor_sample_fetch(bme280_dev);
    if (ret) {
        LOG_ERR("Failed to fetch sensor data: %d", ret);
        data->valid = false;
        return ret;
    }

    // Read temperature
    struct sensor_value temp_val;
    ret = sensor_channel_get(bme280_dev, SENSOR_CHAN_AMBIENT_TEMP, &temp_val);
    if (ret) {
        LOG_ERR("Failed to get temperature: %d", ret);
        data->valid = false;
        return ret;
    }

    // Read humidity
    struct sensor_value humid_val;
    ret = sensor_channel_get(bme280_dev, SENSOR_CHAN_HUMIDITY, &humid_val);
    if (ret) {
        LOG_ERR("Failed to get humidity: %d", ret);
        data->valid = false;
        return ret;
    }

    // Read pressure
    struct sensor_value press_val;
    ret = sensor_channel_get(bme280_dev, SENSOR_CHAN_PRESS, &press_val);
    if (ret) {
        LOG_ERR("Failed to get pressure: %d", ret);
        data->valid = false;
        return ret;
    }

    // Convert sensor values to floats
    data->temperature = sensor_value_to_double(&temp_val);
    data->humidity = sensor_value_to_double(&humid_val);
    data->pressure = sensor_value_to_double(&press_val) / 1000.0; // Convert Pa to hPa
    data->valid = true;

    // Update last reading
    last_reading = *data;

    LOG_DBG("BME280: Temp=%.1f°C, Humid=%.1f%%, Press=%.1fhPa",
            (double)data->temperature, (double)data->humidity, (double)data->pressure);

    return 0;
}

int bme280_sensor_get_data(bme280_data_t *data)
{
    if (data == NULL) {
        return -EINVAL;
    }

    if (!last_reading.valid) {
        LOG_WRN("No valid sensor data available yet");
        return -ENODATA;
    }

    *data = last_reading;
    return 0;
}

bool bme280_sensor_is_ready(void)
{
    return (bme280_dev != NULL && device_is_ready(bme280_dev));
}
