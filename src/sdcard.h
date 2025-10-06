#ifndef SDCARD_H
#define SDCARD_H

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/storage/disk_access.h>
#include <zephyr/fs/fs.h>

#ifdef __cplusplus
extern "C" {
#endif

// SD card mount point
#define SD_MOUNT_POINT "/SD:"

// Function declarations
int sdcard_init(void);
int sdcard_mount(void);
int sdcard_list_files(const char* path);
int sdcard_read_file(const char* filename, uint8_t* buffer, size_t max_size, size_t* bytes_read);
bool sdcard_is_mounted(void);

// Image file structure for BMP images
struct bmp_header {
    uint16_t type;
    uint32_t size;
    uint16_t reserved1;
    uint16_t reserved2;
    uint32_t offset;
    uint32_t dib_header_size;
    int32_t width;
    int32_t height;
    uint16_t planes;
    uint16_t bits_per_pixel;
    uint32_t compression;
    uint32_t image_size;
    int32_t x_pixels_per_meter;
    int32_t y_pixels_per_meter;
    uint32_t colors_used;
    uint32_t colors_important;
} __packed;

// Image functions
int sdcard_load_bmp_image(const char* filename, uint8_t** image_data, int* width, int* height);

#ifdef __cplusplus
}
#endif

#endif /* SDCARD_H */