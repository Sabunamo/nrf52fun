#include "sdcard.h"
#include <ff.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(sdcard, LOG_LEVEL_INF);

// SD card device name (adjust if needed based on device tree)
#define SD_DISK_NAME "SD"

// FAT filesystem work area
static struct fs_mount_t mp = {
    .type = FS_FATFS,
    .mnt_point = SD_MOUNT_POINT,
    .storage_dev = (void *)SD_DISK_NAME,
    .flags = 0,
};

static bool mounted = false;

int sdcard_init(void)
{
    int ret;

    // Check if SD disk is available
    ret = disk_access_init(SD_DISK_NAME);
    if (ret) {
        LOG_ERR("SD card initialization failed: %d", ret);
        return ret;
    }

    LOG_INF("SD card initialized successfully");
    return 0;
}

// Direct FAT filesystem access using FatFs
static FATFS fatfs;
static bool fatfs_mounted = false;

int sdcard_mount_direct(void)
{
    FRESULT fr;

    LOG_INF("Trying direct FatFs mount...");

    // Try different drive paths
    const char* drive_paths[] = {"0:", "SD:", "/", ""};
    int path_count = sizeof(drive_paths) / sizeof(drive_paths[0]);

    for (int i = 0; i < path_count; i++) {
        LOG_INF("Trying drive path: '%s'", drive_paths[i]);
        fr = f_mount(&fatfs, drive_paths[i], 1);  // 1 means mount now

        if (fr == FR_OK) {
            fatfs_mounted = true;
            LOG_INF("Direct FatFs mount successful with path: '%s'", drive_paths[i]);

            // Try to list directory to verify it works and show files
            DIR dir;
            FILINFO fno;
            fr = f_opendir(&dir, drive_paths[i]);
            if (fr == FR_OK) {
                LOG_INF("Directory listing successful");
                LOG_INF("Files on SD card:");

                int file_count = 0;
                while (1) {
                    fr = f_readdir(&dir, &fno);
                    LOG_INF("f_readdir returned: %d", fr);

                    if (fr != FR_OK) {
                        LOG_ERR("Directory read error: %d", fr);
                        break;
                    }

                    if (fno.fname[0] == 0) {
                        LOG_INF("End of directory reached");
                        break;  // End of directory
                    }

                    file_count++;
                    if (fno.fattrib & AM_DIR) {
                        LOG_INF("  DIR:  %s", fno.fname);
                    } else {
                        LOG_INF("  FILE: %s (%llu bytes)", fno.fname, (unsigned long long)fno.fsize);
                    }
                }

                LOG_INF("Total items found: %d", file_count);
                f_closedir(&dir);

                // Try alternative directory listing approach
                LOG_INF("Trying alternative directory approach...");
                fr = f_opendir(&dir, "");  // Try empty path
                if (fr == FR_OK) {
                    LOG_INF("Alternative opendir succeeded");
                    file_count = 0;
                    while (1) {
                        fr = f_readdir(&dir, &fno);
                        if (fr != FR_OK || fno.fname[0] == 0) break;
                        file_count++;
                        LOG_INF("  ALT FILE: %s", fno.fname);
                    }
                    LOG_INF("Alternative method found %d items", file_count);
                    f_closedir(&dir);
                } else {
                    LOG_ERR("Alternative opendir failed: %d", fr);
                }

                return 0;
            } else {
                LOG_ERR("Directory listing failed: %d", fr);
                f_mount(NULL, drive_paths[i], 0); // Unmount
                fatfs_mounted = false;
            }
        } else {
            LOG_ERR("Direct FatFs mount failed with path '%s': %d", drive_paths[i], fr);
        }
    }

    return -1;
}

int sdcard_mount(void)
{
    int ret;

    if (mounted) {
        LOG_INF("SD card already mounted");
        return 0;
    }

    LOG_INF("Attempting to mount FAT filesystem...");
    LOG_INF("Mount point: %s", SD_MOUNT_POINT);
    LOG_INF("Storage device: %s", SD_DISK_NAME);

    // Try multiple mount approaches

    // First try: Standard mount
    ret = fs_mount(&mp);
    if (ret != 0) {
        LOG_ERR("Standard mount failed: %d", ret);

        // Second try: Read-only mount
        LOG_INF("Trying read-only mount...");
        mp.flags = FS_MOUNT_FLAG_READ_ONLY;
        ret = fs_mount(&mp);
        if (ret != 0) {
            LOG_ERR("Read-only mount failed: %d", ret);

            // Third try: Force unmount and remount
            LOG_INF("Trying force unmount/remount...");
            fs_unmount(&mp);
            k_msleep(100);
            mp.flags = 0; // Reset flags
            ret = fs_mount(&mp);
            if (ret != 0) {
                LOG_ERR("Force remount failed: %d", ret);

                // Fourth try: Different mount point
                LOG_INF("Trying different mount point...");
                mp.mnt_point = "/SD";
                ret = fs_mount(&mp);
                if (ret != 0) {
                    LOG_ERR("All mount attempts failed: %d", ret);
                    mp.mnt_point = SD_MOUNT_POINT; // Reset

                    // Fifth try: Direct FatFs access
                    ret = sdcard_mount_direct();
                    if (ret == 0) {
                        mounted = true;
                        LOG_INF("SD card mounted using direct FatFs");
                        return 0;
                    }
                    return ret;
                } else {
                    LOG_INF("Successfully mounted at /SD");
                }
            } else {
                LOG_INF("Successfully mounted after force remount");
            }
        } else {
            LOG_INF("Successfully mounted as read-only");
        }
    } else {
        LOG_INF("Successfully mounted on first attempt");
    }

    mounted = true;
    LOG_INF("SD card mounted successfully");
    return 0;
}

bool sdcard_is_mounted(void)
{
    return mounted;
}

int sdcard_list_files(const char* path)
{
    struct fs_dir_t dir;
    struct fs_dirent entry;
    int ret;

    if (!mounted) {
        LOG_ERR("SD card not mounted");
        return -ENODEV;
    }

    ret = fs_opendir(&dir, path);
    if (ret) {
        LOG_ERR("Failed to open directory %s: %d", path, ret);
        return ret;
    }

    LOG_INF("Files in %s:", path);
    while (1) {
        ret = fs_readdir(&dir, &entry);
        if (ret || entry.name[0] == 0) {
            break;
        }

        if (entry.type == FS_DIR_ENTRY_FILE) {
            LOG_INF("  FILE: %s (size: %zu)", entry.name, entry.size);
        } else if (entry.type == FS_DIR_ENTRY_DIR) {
            LOG_INF("  DIR:  %s", entry.name);
        }
    }

    fs_closedir(&dir);
    return 0;
}

int sdcard_read_file(const char* filename, uint8_t* buffer, size_t max_size, size_t* bytes_read)
{
    struct fs_file_t file;
    int ret;
    ssize_t read_size;

    if (!mounted) {
        LOG_ERR("SD card not mounted");
        return -ENODEV;
    }

    ret = fs_open(&file, filename, FS_O_READ);
    if (ret) {
        LOG_ERR("Failed to open file %s: %d", filename, ret);
        return ret;
    }

    read_size = fs_read(&file, buffer, max_size);
    if (read_size < 0) {
        LOG_ERR("Failed to read file %s: %d", filename, (int)read_size);
        fs_close(&file);
        return read_size;
    }

    *bytes_read = read_size;
    fs_close(&file);

    LOG_INF("Read %zu bytes from %s", *bytes_read, filename);
    return 0;
}

// FatFs-specific BMP loading function
int sdcard_load_bmp_fatfs(const char* filename, uint8_t** image_data, int* width, int* height)
{
    static FIL file;  // Make static to reduce stack usage
    FRESULT fr;
    UINT bytes_read;
    static struct bmp_header header;  // Make static to reduce stack usage
    uint8_t* pixel_data;
    size_t image_size;

    LOG_INF("Attempting to load %s using FatFs", filename);

    // Try different path variations (prioritize Arduino-compatible format)
    const char* path_variations[] = {
        filename,           // Original filename (could be "/WOOF.bmp")
        "/WOOF.bmp",       // Root slash (Arduino format)
        "WOOF.bmp",        // Basic filename
        "0:/WOOF.bmp",     // Drive with slash
        "SD:/WOOF.bmp",    // SD with slash
        "0:WOOF.bmp",      // Drive 0 prefix
        "SD:WOOF.bmp"      // SD drive prefix
    };

    int variation_count = sizeof(path_variations) / sizeof(path_variations[0]);

    for (int i = 0; i < variation_count; i++) {
        LOG_INF("Trying path variation: '%s'", path_variations[i]);
        fr = f_open(&file, path_variations[i], FA_READ);

        if (fr == FR_OK) {
            LOG_INF("Successfully opened file with path: '%s'", path_variations[i]);
            break;
        } else {
            LOG_ERR("Failed to open with path '%s': %d", path_variations[i], fr);
        }
    }

    if (fr != FR_OK) {
        LOG_ERR("All path variations failed for file %s", filename);
        return -fr;
    }

    // Read BMP header
    fr = f_read(&file, &header, sizeof(header), &bytes_read);
    if (fr != FR_OK || bytes_read != sizeof(header)) {
        LOG_ERR("Failed to read BMP header: %d", fr);
        f_close(&file);
        return -fr;
    }

    // Validate BMP header
    if (header.type != 0x4D42) { // "BM" in little endian
        LOG_ERR("Invalid BMP file signature: 0x%04X", header.type);
        f_close(&file);
        return -EINVAL;
    }

    LOG_INF("BMP file size: %u bytes", header.size);
    LOG_INF("Image dimensions: %dx%d", header.width, header.height);

    *width = header.width;
    *height = header.height;

    // For now, just return the header info without loading pixel data
    // This prevents memory issues while testing
    f_close(&file);

    // Set return values
    *image_data = NULL;  // No pixel data for now
    LOG_INF("BMP header read successfully - full loading to be implemented");
    return 0;
}

int sdcard_load_bmp_image(const char* filename, uint8_t** image_data, int* width, int* height)
{
    // Use FatFs if available, otherwise fall back to Zephyr FS
    if (fatfs_mounted) {
        return sdcard_load_bmp_fatfs(filename, image_data, width, height);
    }

    struct bmp_header header;
    struct fs_file_t file;
    int ret;
    ssize_t read_size;
    uint8_t* pixel_data;
    size_t image_size;

    if (!mounted) {
        LOG_ERR("SD card not mounted");
        return -ENODEV;
    }

    ret = fs_open(&file, filename, FS_O_READ);
    if (ret) {
        LOG_ERR("Failed to open BMP file %s: %d", filename, ret);
        return ret;
    }

    // Read BMP header
    read_size = fs_read(&file, &header, sizeof(header));
    if (read_size != sizeof(header)) {
        LOG_ERR("Failed to read BMP header");
        fs_close(&file);
        return -EIO;
    }

    // Validate BMP header
    if (header.type != 0x4D42) { // "BM" in little endian
        LOG_ERR("Invalid BMP file signature: 0x%04X", header.type);
        fs_close(&file);
        return -EINVAL;
    }

    if (header.bits_per_pixel != 24 && header.bits_per_pixel != 16) {
        LOG_ERR("Unsupported BMP format: %d bits per pixel", header.bits_per_pixel);
        fs_close(&file);
        return -ENOTSUP;
    }

    LOG_INF("BMP: %dx%d, %d bits per pixel", header.width, header.height, header.bits_per_pixel);

    // Calculate image size
    if (header.bits_per_pixel == 24) {
        image_size = header.width * header.height * 3; // RGB
    } else if (header.bits_per_pixel == 16) {
        image_size = header.width * header.height * 2; // RGB565
    }

    // Allocate memory for image data
    pixel_data = k_malloc(image_size);
    if (!pixel_data) {
        LOG_ERR("Failed to allocate memory for image data");
        fs_close(&file);
        return -ENOMEM;
    }

    // Seek to image data offset
    ret = fs_seek(&file, header.offset, FS_SEEK_SET);
    if (ret) {
        LOG_ERR("Failed to seek to image data");
        k_free(pixel_data);
        fs_close(&file);
        return ret;
    }

    // Read image data
    read_size = fs_read(&file, pixel_data, image_size);
    if (read_size != image_size) {
        LOG_ERR("Failed to read complete image data: got %d, expected %zu",
                (int)read_size, image_size);
        k_free(pixel_data);
        fs_close(&file);
        return -EIO;
    }

    fs_close(&file);

    *image_data = pixel_data;
    *width = header.width;
    *height = header.height;

    LOG_INF("Successfully loaded BMP image: %dx%d", *width, *height);
    return 0;
}