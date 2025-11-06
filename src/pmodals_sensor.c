#include "pmodals_sensor.h"
#include <zephyr/device.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(pmodals, LOG_LEVEL_INF);

// PmodALS uses ADC081S021 - 8-bit ADC with SPI interface
// CS is active low, data is clocked in on rising edge
#define PMODALS_SPI_FREQ    1000000U  // 1 MHz max for ADC081S021

static const struct device *spi_dev = NULL;
static struct spi_config spi_cfg;
static pmodals_data_t last_reading = {0};

int pmodals_init(void)
{
    // Get SPI device for PmodALS (shares SPI4 with display)
    spi_dev = DEVICE_DT_GET(DT_NODELABEL(spi4));

    if (spi_dev == NULL) {
        LOG_ERR("PmodALS: SPI device not found");
        return -ENODEV;
    }

    if (!device_is_ready(spi_dev)) {
        LOG_ERR("PmodALS: SPI device not ready");
        spi_dev = NULL;
        return -ENODEV;
    }

    // Configure SPI for ADC081S021
    spi_cfg.frequency = PMODALS_SPI_FREQ;
    spi_cfg.operation = SPI_WORD_SET(8) | SPI_TRANSFER_MSB |
                        SPI_MODE_CPOL | SPI_MODE_CPHA;  // Mode 3
    spi_cfg.slave = 1;  // Device 1 on shared SPI4 bus (display is device 0)
    // CS is configured in devicetree (P1.07 for PmodALS)

    LOG_INF("PmodALS sensor initialized successfully");

    // Initialize last_reading
    last_reading.valid = false;
    last_reading.raw_value = 0;
    last_reading.lux = 0;
    last_reading.brightness_pct = 50;

    return 0;
}

int pmodals_read(pmodals_data_t *data)
{
    if (data == NULL) {
        return -EINVAL;
    }

    if (spi_dev == NULL || !device_is_ready(spi_dev)) {
        LOG_ERR("PmodALS: SPI device not ready for reading");
        data->valid = false;
        return -ENODEV;
    }

    // ADC081S021 protocol:
    // Send 2 dummy bytes, receive 2 bytes
    // Byte 1: bits [11:4] of result (we only use 8 bits)
    // Byte 2: bits [3:0] of result in upper nibble
    uint8_t tx_buf[2] = {0xFF, 0xFF};  // Dummy data
    uint8_t rx_buf[2] = {0};

    struct spi_buf tx_spi_buf = {
        .buf = tx_buf,
        .len = sizeof(tx_buf)
    };

    struct spi_buf rx_spi_buf = {
        .buf = rx_buf,
        .len = sizeof(rx_buf)
    };

    struct spi_buf_set tx_spi_buf_set = {
        .buffers = &tx_spi_buf,
        .count = 1
    };

    struct spi_buf_set rx_spi_buf_set = {
        .buffers = &rx_spi_buf,
        .count = 1
    };

    // Perform SPI transaction
    int ret = spi_transceive(spi_dev, &spi_cfg, &tx_spi_buf_set, &rx_spi_buf_set);
    if (ret) {
        LOG_ERR("PmodALS: SPI transaction failed: %d", ret);
        data->valid = false;
        return ret;
    }

    // Extract 8-bit value from the two received bytes
    // ADC081S021 sends 12-bit result, we use the upper 8 bits
    data->raw_value = rx_buf[0];

    // Convert raw ADC value to approximate lux
    // TEMT6000 typical: 10 uA per lux
    // With ADC reference and scaling, rough approximation:
    // 0 = dark (~0 lux), 255 = bright (~1000 lux)
    data->lux = (uint16_t)((data->raw_value * 1000UL) / 255);

    // Calculate suggested brightness percentage
    data->brightness_pct = pmodals_lux_to_brightness(data->lux);
    data->valid = true;

    // Update last reading
    last_reading = *data;

    LOG_DBG("PmodALS: Raw=%d, Lux=%d, Brightness=%d%%",
            data->raw_value, data->lux, data->brightness_pct);

    return 0;
}

int pmodals_get_data(pmodals_data_t *data)
{
    if (data == NULL) {
        return -EINVAL;
    }

    if (!last_reading.valid) {
        LOG_WRN("No valid PmodALS data available yet");
        return -ENODATA;
    }

    *data = last_reading;
    return 0;
}

bool pmodals_is_ready(void)
{
    return (spi_dev != NULL && device_is_ready(spi_dev));
}

uint8_t pmodals_lux_to_brightness(uint16_t lux)
{
    // Brightness curve mapping lux to display brightness percentage
    // This provides a comfortable viewing experience across lighting conditions

    if (lux < 10) {
        // Very dark: 20-30% brightness
        return 20 + (lux * 10) / 10;  // 20-30%
    } else if (lux < 50) {
        // Dark: 30-50% brightness
        return 30 + ((lux - 10) * 20) / 40;  // 30-50%
    } else if (lux < 200) {
        // Normal indoor: 50-75% brightness
        return 50 + ((lux - 50) * 25) / 150;  // 50-75%
    } else if (lux < 500) {
        // Bright indoor: 75-90% brightness
        return 75 + ((lux - 200) * 15) / 300;  // 75-90%
    } else {
        // Very bright / outdoor: 90-100% brightness
        uint16_t excess = (lux > 1000) ? 1000 : lux;
        return 90 + ((excess - 500) * 10) / 500;  // 90-100%
    }
}
