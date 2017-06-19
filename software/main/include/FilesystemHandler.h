#pragma once
extern "C"
{
    #include "esp_vfs.h"
    #include "esp_vfs_fat.h"
    #include "esp_system.h"
}
#include <cstdint>

class FilesystemHandler;

class FilesystemHandler
{
    public:
        static void register_filesystem(const char *partition_name, const char * mountpt);
        // Handle of the wear levelling library instance
        static wl_handle_t s_wl_handle;

};
