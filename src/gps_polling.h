/* GPS polling thread for nRF5340-DK (interrupt-driven UART RX is unreliable) */

/* Polling thread variables */
static uint32_t poll_count = 0;
static uint32_t total_bytes_received_poll = 0;
static uint32_t poll_in_success = 0;
static uint8_t last_poll_error = 0;

#define DEBUG_NMEA_COUNT 5
static char debug_nmea[DEBUG_NMEA_COUNT][80];
static int debug_nmea_index = 0;

#define GPS_THREAD_STACK_SIZE 2048
#define GPS_THREAD_PRIORITY 7

/**
 * @brief GPS polling thread for nRF5340-DK
 *
 * Polls UART for GPS data instead of using interrupts (interrupt-driven UART RX
 * is unreliable on nRF5340-DK). Processes NMEA sentences and updates GPS data structure.
 */
static void gps_poll_thread(void *p1, void *p2, void *p3)
{
    uint8_t byte;

    printk("GPS polling thread started - waiting for UART init\n");

    // Wait for UART to be initialized
    while (!gps_uart || !device_is_ready(gps_uart)) {
        k_msleep(100);
    }

    printk("GPS polling thread - UART ready, starting to poll\n");

    uint32_t last_stats_print = 0;

    while (1) {
        poll_count++;

        // Poll for incoming data - keep polling while data available
        int poll_ret;
        while ((poll_ret = uart_poll_in(gps_uart, &byte)) == 0) {
            poll_in_success++;
            total_bytes_received_poll++;

            // Process only printable ASCII characters for NMEA data
            if (byte >= 32 && byte <= 126) {
                if (gps_buffer_pos < GPS_BUFFER_SIZE - 1) {
                    gps_buffer[gps_buffer_pos] = byte;
                    gps_buffer_pos++;
                } else {
                    // Buffer overflow protection - reset to prevent corruption
                    gps_buffer_pos = 0;
                }
            } else if (byte == '\r') {
                // Ignore carriage return characters
                continue;
            } else if (byte == '\n') {
                // End of NMEA sentence - process if valid
                if (gps_buffer_pos > 0) {
                    gps_buffer[gps_buffer_pos] = '\0';

                    // Validate NMEA sentence format (must start with $ and have minimum length)
                    if (gps_buffer[0] == '$' && gps_buffer_pos > 6) {
                        // Store for debug display
                        strncpy(debug_nmea[debug_nmea_index], gps_buffer, 79);
                        debug_nmea[debug_nmea_index][79] = '\0';
                        debug_nmea_index = (debug_nmea_index + 1) % DEBUG_NMEA_COUNT;

                        printk("GPS NMEA: %s\n", gps_buffer);
                        process_nmea_sentence(gps_buffer);
                    }

                    gps_buffer_pos = 0;
                }
            }
            // Ignore other invalid characters without resetting buffer
        }

        // Store last error code if any
        if (poll_ret != 0) {
            last_poll_error = (uint8_t)(-poll_ret);
        }

        // Print statistics every 5 seconds
        if (poll_count - last_stats_print >= 5000) {  // 5000 * 1ms = 5 seconds
            printk("GPS Stats: %u bytes received, %u polls\n", total_bytes_received_poll, poll_in_success);
            last_stats_print = poll_count;
        }

        // Poll at ~1000Hz (1ms interval) - faster to prevent buffer overrun
        k_msleep(1);
    }
}

K_THREAD_DEFINE(gps_poll_tid, GPS_THREAD_STACK_SIZE,
                gps_poll_thread, NULL, NULL, NULL,
                GPS_THREAD_PRIORITY, 0, 0);
