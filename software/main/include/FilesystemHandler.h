#pragma once
#define LOG_PAGE_SIZE 256
extern "C"
{
    #include "spiffs.h"
}
#include <cstdint>

class FilesystemHandler;

class FilesystemHandler
{
    public:
        static FilesystemHandler *get_instance() { return FilesystemHandler::instance; };
        static FilesystemHandler *get_instance(size_t _addr, size_t _size, char * _mountpt);
        size_t addr;
        size_t size;
        char * mountpt;
        bool init_spiffs();
        spiffs fs;
    private:
        static FilesystemHandler *instance;
        uint8_t spiffs_work_buf[LOG_PAGE_SIZE*2];
        uint8_t spiffs_fds[32*4];
        uint8_t spiffs_cache_buf[(LOG_PAGE_SIZE+32)*4];
        FilesystemHandler(size_t _addr, size_t _size, char * _mountpt);


};
