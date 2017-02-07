#pragma once
#define LOG_PAGE_SIZE 256
extern "C"
{
    #include "spiffs.h"
}
#include <cstdint>

class FilesystemHandler
{
    public:
        FilesystemHandler(size_t _addr, size_t _size, char * _mountpt);
        size_t addr;
        size_t size;
        char * mountpt;
        bool init_spiffs();
    private:
        spiffs fs;
        uint8_t spiffs_work_buf[LOG_PAGE_SIZE*2];
        uint8_t spiffs_fds[32*4];
        uint8_t spiffs_cache_buf[(LOG_PAGE_SIZE+32)*4];

};
