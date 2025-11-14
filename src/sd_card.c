/*
 * SD Card Module - Implementation
 * Handles SD card initialization, file operations, and media playback
 */

#include "sd_card.h"
#include <zephyr/logging/log.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/fs/fs.h>
#include <errno.h>
#include <string.h>
#include <strings.h>

LOG_MODULE_REGISTER(sd_card, LOG_LEVEL_DBG);

#define PWM_SPEAKER_NODE DT_NODELABEL(pwm0)
#define BUFFER_SIZE 2048

/* RGB565 color conversion */
#define RGB565(r, g, b) ((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | (((b) & 0xF8) >> 3))
#define COLOR_BLACK RGB565(0, 0, 0)

/* Static variables */
static FATFS fat_fs;
static struct fs_mount_t mp = {
	.type = FS_FATFS,
	.fs_data = &fat_fs,
};

static const struct device *pwm_dev = NULL;
static const struct device *g_display_dev = NULL;

/* Forward declarations */
static void fill_screen(const struct device *display_dev, uint16_t color);

/* Initialize SD card and filesystem */
int sd_card_init(void)
{
	int ret;
	FRESULT fres;

	LOG_INF("Starting SD card initialization...");
	LOG_INF("Calling disk_access_init(\"%s\")...", DISK_DRIVE_NAME);

	ret = disk_access_init(DISK_DRIVE_NAME);
	if (ret != 0) {
		LOG_ERR("disk_access_init() failed with error: %d", ret);
		LOG_ERR("Error codes: -5=EIO, -116=ENOTSUP, -134=EILSEQ");
		return ret;
	}

	LOG_INF("SD card initialized successfully!");

	/* Mount filesystem using SD: drive prefix */
	fres = f_mount(&fat_fs, "SD:", 1);
	if (fres != FR_OK) {
		LOG_ERR("f_mount failed: %d", fres);
		LOG_ERR("FatFS error codes: 11=FR_NOT_READY, 13=FR_NO_FILESYSTEM");
		LOG_INF("Trying to mount anyway - some cards need operations first");
		/* Don't fail here - try to continue and see if operations work */
	} else {
		LOG_INF("Filesystem mounted successfully at SD:");
	}

	/* Initialize PWM device for audio */
	pwm_dev = DEVICE_DT_GET(PWM_SPEAKER_NODE);
	if (!device_is_ready(pwm_dev)) {
		LOG_WRN("PWM device not ready - audio playback unavailable");
	}

	return 0;
}

/* Get SD card size information */
int sd_card_get_size(uint32_t *block_count, uint32_t *block_size)
{
	int ret;

	ret = disk_access_ioctl(DISK_DRIVE_NAME, DISK_IOCTL_GET_SECTOR_COUNT, block_count);
	if (ret != 0) {
		LOG_ERR("Failed to get sector count: %d", ret);
		return ret;
	}

	ret = disk_access_ioctl(DISK_DRIVE_NAME, DISK_IOCTL_GET_SECTOR_SIZE, block_size);
	if (ret != 0) {
		LOG_ERR("Failed to get sector size: %d", ret);
		return ret;
	}

	return 0;
}

/* Find first WAV file on SD card */
int sd_card_find_wav_file(char *path_buffer, size_t buffer_size)
{
	FRESULT res;
	DIR dir;
	FILINFO fno;

	if (!path_buffer || buffer_size < 64) {
		return -1;
	}

	path_buffer[0] = 0;

	res = f_opendir(&dir, "SD:");
	if (res != FR_OK) {
		LOG_ERR("Failed to open directory SD:");
		return -1;
	}

	while (1) {
		res = f_readdir(&dir, &fno);
		if (res != FR_OK || fno.fname[0] == 0) {
			break;
		}

		/* Check if it's a WAV file */
		int len = strlen(fno.fname);
		if (len > 4 && (strcasecmp(&fno.fname[len-4], ".wav") == 0)) {
			snprintf(path_buffer, buffer_size, "SD:/%s", fno.fname);
			LOG_INF("Found WAV file: %s", fno.fname);
			f_closedir(&dir);
			return 0;
		}
	}

	f_closedir(&dir);
	return -1;
}

/* Set display device for BMP rendering */
void sd_card_set_display_device(const struct device *display_dev)
{
	g_display_dev = display_dev;
}

/* Helper function - fill screen with color */
static void fill_screen(const struct device *display_dev, uint16_t color)
{
	struct display_capabilities capabilities;
	display_get_capabilities(display_dev, &capabilities);

	uint16_t width = capabilities.x_resolution;
	uint16_t height = capabilities.y_resolution;

	uint16_t *line_buf = k_malloc(width * sizeof(uint16_t));
	if (!line_buf) {
		return;
	}

	for (int x = 0; x < width; x++) {
		line_buf[x] = color;
	}

	struct display_buffer_descriptor buf_desc = {
		.width = width,
		.height = 1,
		.pitch = width,
		.buf_size = width * sizeof(uint16_t),
	};

	for (int y = 0; y < height; y++) {
		display_write(display_dev, 0, y, &buf_desc, line_buf);
	}

	k_free(line_buf);
}


/* WAV and BMP implementation functions */

/* Play audio from WAV file - PWM frequency matches sample rate automatically */
int sd_card_play_wav_file(const char *filename, uint32_t pwm_freq_hz)
{
	/* Note: pwm_freq_hz parameter is ignored - we use the file's sample rate */
	(void)pwm_freq_hz;  /* Suppress unused parameter warning */

	FIL file;
	FRESULT res;
	UINT bytes_read;
	wav_riff_t riff;
	uint32_t sample_rate = 0;
	uint16_t num_channels = 0;
	uint16_t bits_per_sample = 0;
	uint32_t data_size = 0;

	LOG_INF("Opening audio file: %s", filename);

	res = f_open(&file, filename, FA_READ);
	if (res != FR_OK) {
		LOG_ERR("Failed to open %s: %d", filename, res);
		return -1;
	}

	/* Read RIFF header */
	res = f_read(&file, &riff, sizeof(wav_riff_t), &bytes_read);
	if (res != FR_OK || bytes_read != sizeof(wav_riff_t)) {
		LOG_ERR("Failed to read RIFF header: %d", res);
		f_close(&file);
		return -1;
	}

	/* Validate RIFF header */
	if (memcmp(riff.riff, "RIFF", 4) != 0 ||
	    memcmp(riff.wave, "WAVE", 4) != 0) {
		LOG_ERR("Invalid WAV file format");
		f_close(&file);
		return -1;
	}

	LOG_INF("Valid RIFF/WAVE header found, file size: %u bytes", riff.file_size);
	LOG_INF("Current file position: %lu", (unsigned long)f_tell(&file));

	/* Find fmt chunk */
	bool fmt_found = false;
	uint8_t chunk_id[4];
	uint32_t chunk_size;
	int chunk_count = 0;

	while (!fmt_found && chunk_count < 20) {  /* Limit search to prevent infinite loop */
		chunk_count++;

		res = f_read(&file, chunk_id, 4, &bytes_read);
		if (res != FR_OK || bytes_read != 4) {
			LOG_ERR("Failed to read chunk ID at position %lu", (unsigned long)f_tell(&file));
			f_close(&file);
			return -1;
		}

		res = f_read(&file, &chunk_size, 4, &bytes_read);
		if (res != FR_OK || bytes_read != 4) {
			LOG_ERR("Failed to read chunk size");
			f_close(&file);
			return -1;
		}

		LOG_INF("Found chunk '%.4s' at position %lu, size: %u bytes",
		        chunk_id, (unsigned long)(f_tell(&file) - 8), chunk_size);

		if (memcmp(chunk_id, "fmt ", 4) == 0) {
			/* Read format chunk data */
			uint16_t audio_format;
			res = f_read(&file, &audio_format, 2, &bytes_read);
			res |= f_read(&file, &num_channels, 2, &bytes_read);
			res |= f_read(&file, &sample_rate, 4, &bytes_read);
			uint32_t byte_rate;
			uint16_t block_align;
			res |= f_read(&file, &byte_rate, 4, &bytes_read);
			res |= f_read(&file, &block_align, 2, &bytes_read);
			res |= f_read(&file, &bits_per_sample, 2, &bytes_read);

			if (res != FR_OK) {
				LOG_ERR("Failed to read fmt chunk data");
				f_close(&file);
				return -1;
			}

			LOG_INF("Format chunk parsed:");
			LOG_INF("  Audio format: %u (1=PCM)", audio_format);
			LOG_INF("  Sample rate: %u Hz", sample_rate);
			LOG_INF("  Channels: %u", num_channels);
			LOG_INF("  Bits per sample: %u", bits_per_sample);
			LOG_INF("  Byte rate: %u", byte_rate);
			LOG_INF("  Block align: %u", block_align);

			/* Validate format */
			if (audio_format != 1) {
				LOG_ERR("Unsupported audio format: %u (only PCM=1 is supported)", audio_format);
				f_close(&file);
				return -1;
			}

			/* Skip any remaining bytes in fmt chunk (some have 18 or 40 bytes) */
			uint32_t bytes_read_from_fmt = 16;
			if (chunk_size > bytes_read_from_fmt) {
				uint32_t skip_bytes = chunk_size - bytes_read_from_fmt;
				LOG_INF("Skipping %u extra bytes in fmt chunk", skip_bytes);
				f_lseek(&file, f_tell(&file) + skip_bytes);
			}

			fmt_found = true;
		} else {
			/* Skip this chunk - make sure size is reasonable */
			if (chunk_size > 10000000) {  /* 10MB sanity check */
				LOG_ERR("Chunk size too large: %u bytes - file may be corrupted", chunk_size);
				f_close(&file);
				return -1;
			}
			LOG_INF("Skipping chunk '%.4s' (%u bytes)", chunk_id, chunk_size);
			f_lseek(&file, f_tell(&file) + chunk_size);
		}
	}

	if (!fmt_found) {
		LOG_ERR("fmt chunk not found after checking %d chunks", chunk_count);
		f_close(&file);
		return -1;
	}

	/* Find data chunk */
	bool data_found = false;
	chunk_count = 0;

	while (!data_found && chunk_count < 20) {
		chunk_count++;

		res = f_read(&file, chunk_id, 4, &bytes_read);
		if (res != FR_OK || bytes_read != 4) {
			LOG_ERR("Failed to find data chunk - EOF reached");
			f_close(&file);
			return -1;
		}

		res = f_read(&file, &data_size, 4, &bytes_read);
		if (res != FR_OK || bytes_read != 4) {
			LOG_ERR("Failed to read data chunk size");
			f_close(&file);
			return -1;
		}

		LOG_INF("Found chunk '%.4s' at position %lu, size: %u bytes",
		        chunk_id, (unsigned long)(f_tell(&file) - 8), data_size);

		if (memcmp(chunk_id, "data", 4) == 0) {
			LOG_INF("DATA chunk found! Audio data starts at position %lu", (unsigned long)f_tell(&file));
			data_found = true;
		} else {
			/* Skip this chunk */
			if (data_size > 10000000) {
				LOG_ERR("Chunk size too large: %u bytes", data_size);
				f_close(&file);
				return -1;
			}
			LOG_INF("Skipping chunk '%.4s' (%u bytes)", chunk_id, data_size);
			f_lseek(&file, f_tell(&file) + data_size);
		}
	}

	if (!data_found) {
		LOG_ERR("data chunk not found after checking %d chunks", chunk_count);
		f_close(&file);
		return -1;
	}

	/* Validate audio parameters */
	if (bits_per_sample != 8 && bits_per_sample != 16) {
		LOG_ERR("Unsupported bit depth: %u (only 8-bit or 16-bit supported)", bits_per_sample);
		f_close(&file);
		return -1;
	}

	if (sample_rate == 0) {
		LOG_ERR("Invalid sample rate: 0");
		f_close(&file);
		return -1;
	}

	if (data_size == 0) {
		LOG_ERR("No audio data in file");
		f_close(&file);
		return -1;
	}

	/* Warn if file is too large for good quality */
	if (data_size > 500000) {
		LOG_WRN("Audio file is large (%u bytes). Expect slow SD card reads.", data_size);
		LOG_WRN("Recommended: Use 8kHz mono 8-bit PCM for better quality.");
	}

	/* Check PWM device */
	if (!device_is_ready(pwm_dev)) {
		LOG_ERR("PWM device not ready");
		f_close(&file);
		return -1;
	}

	LOG_INF("Starting audio playback...");
	LOG_INF("NOTE: PWM audio output is low power. Use an amplifier for better volume.");

	/* For better audio quality, we need to downsample or use lower sample rates */
	/* Recommended: 8kHz or 11kHz mono, 8-bit or 16-bit PCM */

	/* Calculate timing - use k_msleep for more stable timing */
	uint32_t sample_period_us = 1000000 / sample_rate;

	/* Use PWM frequency matching sample rate - original approach */
	uint32_t pwm_freq_to_use = sample_rate;  /* Match sample rate */
	uint32_t pwm_period_ns = 1000000000 / pwm_freq_to_use;
	uint32_t bytes_per_sample = (bits_per_sample / 8) * num_channels;

	LOG_INF("Using PWM frequency: %u Hz", pwm_freq_to_use);
	LOG_INF("Sample rate: %u Hz, PWM period: %u ns", sample_rate, pwm_period_ns);

	/* Audio playback buffer - larger buffer to reduce SD card read frequency */
	#define BUFFER_SIZE 2048
	uint8_t buffer[BUFFER_SIZE];
	uint32_t total_samples = data_size / bytes_per_sample;
	uint32_t samples_played = 0;
	uint32_t last_progress = 0;

	LOG_INF("Playing %u samples at %u Hz", total_samples, sample_rate);

	/* Play complete audio file - no modifications, no skipping */
	LOG_INF("Will play complete audio: %u samples (%.1f seconds)",
	        total_samples, (float)total_samples / sample_rate);

	int64_t next_sample_time = k_uptime_get() * 1000;  /* Convert to us */

	while (samples_played < total_samples) {
		/* Read audio data */
		res = f_read(&file, buffer, BUFFER_SIZE, &bytes_read);
		if (res != FR_OK || bytes_read == 0) {
			break;
		}

		/* Play samples */
		for (uint32_t i = 0; i < bytes_read; i += bytes_per_sample) {
			if (bits_per_sample == 16) {
				/* 16-bit signed (-32768 to 32767) */
				int16_t signed_sample = *(int16_t*)&buffer[i];

				/* Convert signed 16-bit to unsigned 16-bit */
				/* -32768 maps to 0, 0 maps to 32768, 32767 maps to 65535 */
				uint32_t unsigned_sample = (uint32_t)((int32_t)signed_sample + 32768);

				/* Convert to PWM duty cycle */
				/* Simple direct mapping: duty = (sample / 65536) * period */
				uint32_t pwm_duty = (unsigned_sample * pwm_period_ns) >> 16;

				/* Set PWM - duty cycle should be 0 to pwm_period_ns */
				pwm_set(pwm_dev, 0, pwm_period_ns, pwm_duty, 0);

			} else if (bits_per_sample == 8) {
				/* 8-bit unsigned (0-255, center at 128) */
				uint8_t sample_8bit = buffer[i];

				/* Convert to PWM duty cycle */
				uint32_t pwm_duty = (sample_8bit * pwm_period_ns) >> 8;

				/* Set PWM */
				pwm_set(pwm_dev, 0, pwm_period_ns, pwm_duty, 0);
			} else {
				continue;
			}

			/* Wait until the right time for the next sample */
			next_sample_time += sample_period_us;
			int64_t current_time = k_uptime_get() * 1000;  /* us */
			int64_t wait_time = next_sample_time - current_time;

			if (wait_time > 0) {
				if (wait_time > 1000) {
					/* Sleep for milliseconds if wait is long enough */
					k_msleep(wait_time / 1000);
					/* Busy wait for the remaining microseconds */
					k_busy_wait(wait_time % 1000);
				} else {
					/* Just busy wait for short delays */
					k_busy_wait(wait_time);
				}
			}
			/* If we're running behind, skip the wait */

			samples_played++;

			/* Progress update every 10% */
			uint32_t progress = (samples_played * 100) / total_samples;
			if (progress >= last_progress + 10) {
				LOG_INF("Playback progress: %u%% (%u/%u samples)",
				        progress, samples_played, total_samples);
				last_progress = progress;
			}
		}
	}

	/* Stop PWM */
	pwm_set(pwm_dev, 0, pwm_period_ns, 0, 0);

	f_close(&file);
	LOG_INF("Playback complete! Played %u samples", samples_played);

	return 0;
}

int sd_card_display_bmp_file(const char *filename) {
	FIL file;
	FRESULT res;
	UINT bytes_read;
	BITMAPFILEHEADER fileHeader;
	BITMAPINFOHEADER infoHeader;

	LOG_INF("Displaying BMP: %s", filename);

	res = f_open(&file, filename, FA_READ);
	if (res != FR_OK) {
		LOG_ERR("f_open failed with error %d", res);
		return -1;
	}

	/* Read file header */
	res = f_read(&file, &fileHeader, sizeof(fileHeader), &bytes_read);
	if (res != FR_OK || bytes_read != sizeof(fileHeader)) {
		LOG_ERR("Failed to read file header");
		f_close(&file);
		return -1;
	}

	/* Check BMP signature */
	if (fileHeader.bfType != 0x4D42) {  /* 'BM' */
		LOG_ERR("Not a valid BMP file (signature: 0x%04X)", fileHeader.bfType);
		f_close(&file);
		return -1;
	}

	/* Read info header */
	res = f_read(&file, &infoHeader, sizeof(infoHeader), &bytes_read);
	if (res != FR_OK || bytes_read != sizeof(infoHeader)) {
		LOG_ERR("Failed to read info header");
		f_close(&file);
		return -1;
	}

	LOG_INF("BMP Info: %dx%d, %d bits per pixel",
		infoHeader.biWidth, infoHeader.biHeight, infoHeader.biBitCount);

	/* Only support 24-bit BMPs for now */
	if (infoHeader.biBitCount != 24) {
		LOG_ERR("Only 24-bit BMPs are supported (got %d-bit)", infoHeader.biBitCount);
		f_close(&file);
		return -1;
	}

	/* Seek to pixel data */
	f_lseek(&file, fileHeader.bfOffBits);

	/* Calculate dimensions */
	int width = infoHeader.biWidth;
	int height = infoHeader.biHeight > 0 ? infoHeader.biHeight : -infoHeader.biHeight;
	bool bottomUp = infoHeader.biHeight > 0;

	/* Get display size for clipping */
	struct display_capabilities capabilities;
	display_get_capabilities(g_display_dev, &capabilities);
	int display_width = capabilities.x_resolution;
	int display_height = capabilities.y_resolution;

	/* Display at original size starting from (0,0) - no centering */
	int offset_x = 0;
	int offset_y = 0;

	/* Clear screen first */
	fill_screen(g_display_dev, COLOR_BLACK);

	/* BMP rows are padded to 4-byte boundaries */
	int row_size = ((width * 3 + 3) / 4) * 4;
	uint8_t *row_buf = k_malloc(row_size);
	uint16_t *pixel_buf = k_malloc(width * sizeof(uint16_t));

	if (!row_buf || !pixel_buf) {
		LOG_ERR("Failed to allocate memory");
		if (row_buf) k_free(row_buf);
		if (pixel_buf) k_free(pixel_buf);
		f_close(&file);
		return -1;
	}

	struct display_buffer_descriptor buf_desc = {
		.width = width,
		.height = 1,
		.pitch = width,
		.buf_size = width * sizeof(uint16_t),
	};

	/* Read and display line by line */
	for (int y = 0; y < height && y < display_height; y++) {
		int display_y = bottomUp ? (height - 1 - y) : y;

		res = f_read(&file, row_buf, row_size, &bytes_read);
		if (res != FR_OK) {
			LOG_ERR("Failed to read row %d", y);
			break;
		}

		/* Convert BGR to RGB565 (swap R and B for correct colors) */
		for (int x = 0; x < width && x < display_width; x++) {
			uint8_t b = row_buf[x * 3];
			uint8_t g = row_buf[x * 3 + 1];
			uint8_t r = row_buf[x * 3 + 2];
			pixel_buf[x] = RGB565(b, g, r);  /* Swap R and B */
		}

		/* Write to display */
		display_write(g_display_dev, offset_x, offset_y + display_y, &buf_desc, pixel_buf);
	}

	k_free(row_buf);
	k_free(pixel_buf);
	f_close(&file);

	LOG_INF("BMP displayed successfully!");
	return 0;
}
