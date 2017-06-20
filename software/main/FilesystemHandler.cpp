#include "FilesystemHandler.h"

extern "C"
{
}

static const char *TAG = "FilesystemHandler";

wl_handle_t FilesystemHandler::s_wl_handle = WL_INVALID_HANDLE;

void FilesystemHandler::register_filesystem(const char *partition_name, const char * mountpt)
{
    ESP_LOGI(TAG, "Mounting FAT filesystem");

    esp_vfs_fat_mount_config_t mount_config;
    mount_config.format_if_mount_failed = false;
    mount_config.max_files = 20;
    esp_err_t err = esp_vfs_fat_spiflash_mount(mountpt, partition_name, &mount_config, &s_wl_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount FATFS (0x%x)", err);
        return;
    }
}
