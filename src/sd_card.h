/*
 * SD Card Module - Header
 * Handles SD card initialization, file operations, and media playback
 */

#ifndef SD_CARD_H
#define SD_CARD_H

#include <zephyr/kernel.h>
#include <zephyr/storage/disk_access.h>
#include <zephyr/drivers/display.h>
#include <ff.h>
#include <stdint.h>
#include <stdbool.h>

/* SD card configuration */
#define DISK_DRIVE_NAME "SD"
#define DISK_MOUNT_PT "/SD:"

/* WAV file structures */
typedef struct {
	char riff[4];           /* "RIFF" */
	uint32_t file_size;     /* File size - 8 bytes */
	char wave[4];           /* "WAVE" */
} wav_riff_t;

typedef struct {
	char fmt[4];            /* "fmt " */
	uint32_t chunk_size;    /* Format chunk size */
	uint16_t audio_format;  /* Audio format (1 = PCM) */
	uint16_t num_channels;  /* Number of channels */
	uint32_t sample_rate;   /* Sample rate */
	uint32_t byte_rate;     /* Byte rate */
	uint16_t block_align;   /* Block align */
	uint16_t bits_per_sample; /* Bits per sample */
} wav_fmt_t;

typedef struct {
	char data[4];           /* "data" */
	uint32_t data_size;     /* Data size */
} wav_data_t;

/* BMP file structures */
#pragma pack(push, 1)
typedef struct {
	uint16_t bfType;
	uint32_t bfSize;
	uint16_t bfReserved1;
	uint16_t bfReserved2;
	uint32_t bfOffBits;
} BITMAPFILEHEADER;

typedef struct {
	uint32_t biSize;
	int32_t biWidth;
	int32_t biHeight;
	uint16_t biPlanes;
	uint16_t biBitCount;
	uint32_t biCompression;
	uint32_t biSizeImage;
	int32_t biXPelsPerMeter;
	int32_t biYPelsPerMeter;
	uint32_t biClrUsed;
	uint32_t biClrImportant;
} BITMAPINFOHEADER;
#pragma pack(pop)

/* SD card functions */

/**
 * @brief Initialize SD card and filesystem
 * @return 0 on success, negative errno on failure
 */
int sd_card_init(void);

/**
 * @brief Get SD card size information
 * @param block_count Pointer to store block count
 * @param block_size Pointer to store block size
 * @return 0 on success, negative errno on failure
 */
int sd_card_get_size(uint32_t *block_count, uint32_t *block_size);

/**
 * @brief Find first WAV file on SD card
 * @param path_buffer Buffer to store path (must be at least 64 bytes)
 * @param buffer_size Size of path buffer
 * @return 0 if found, -1 if not found
 */
int sd_card_find_wav_file(char *path_buffer, size_t buffer_size);

/**
 * @brief Play WAV audio file with specified PWM frequency
 * @param filename Path to WAV file (e.g., "SD:/audio.wav")
 * @param pwm_freq_hz PWM frequency in Hz (e.g., 62500)
 * @return 0 on success, negative errno on failure
 */
int sd_card_play_wav_file(const char *filename, uint32_t pwm_freq_hz);

/**
 * @brief Display BMP image file on screen
 * @param filename Path to BMP file (e.g., "SD:woof.bmp")
 * @return 0 on success, negative errno on failure
 */
int sd_card_display_bmp_file(const char *filename);

/**
 * @brief Set the global display device for BMP rendering
 * @param display_dev Pointer to display device
 */
void sd_card_set_display_device(const struct device *display_dev);

#endif /* SD_CARD_H */
