#include "SettingsReader.h"

extern "C"
{
    #include "spiffs.h"
    #include "esp_spiffs.h"
}

xSemaphoreHandle SettingsReader::access_lock;

bool SettingsReader::initialised = false;

bool SettingsReader::init()
{
    if(SettingsReader::initialised == false)
    {
        esp_spiffs_init();
        if (esp_spiffs_mount() != SPIFFS_OK)
        {
            printf("Error mount SPIFFS\n");
            return false;
        }
        vSemaphoreCreateBinary(SettingsReader::access_lock);
        if(SettingsReader::access_lock != NULL)
        {
            SettingsReader::initialised = true;
            return false;
        }
    }

    spiffs_DIR d;
    struct spiffs_dirent e;
    struct spiffs_dirent *pe = &e;

    SPIFFS_opendir(&fs, "/", &d);
    while ((pe = SPIFFS_readdir(&d, pe))) {
      printf("%s [%04x] size:%i\n", pe->name, pe->obj_id, pe->size);
    }
    SPIFFS_closedir(&d);
    return true;
}


bool SettingsReader::readfile(const char *filepath, char *buf, uint16_t max_len)
{
    if(!SettingsReader::file_exists(filepath)) return false;
    xSemaphoreTake(SettingsReader::access_lock, portMAX_DELAY);
    int fd = SPIFFS_open(&fs, filepath, SPIFFS_RDONLY, 0);
    int read_bytes = SPIFFS_read(&fs, fd, buf, max_len);

    if(read_bytes == SPIFFS_ERR_END_OF_OBJECT) read_bytes = 0;

    buf[read_bytes] = '\0';
    //printf("%s\n", buf);
    SPIFFS_close(&fs, fd);
    xSemaphoreGive(SettingsReader::access_lock);
    return true;
}


bool SettingsReader::writefile(const char *filepath, char *buf, uint16_t write_len)
{
    xSemaphoreTake(SettingsReader::access_lock, portMAX_DELAY);
    int fd = SPIFFS_open(&fs, filepath, SPIFFS_TRUNC | SPIFFS_CREAT, 0);
    int written = SPIFFS_write(&fs, fd, buf, write_len);
    SPIFFS_close(&fs, fd);
    xSemaphoreGive(SettingsReader::access_lock);

    if(written == write_len) return true;
    else return false;
}


bool SettingsReader::file_exists(const char *filepath)
{
    xSemaphoreTake(SettingsReader::access_lock, portMAX_DELAY);
    spiffs_stat s;
    int res = SPIFFS_stat(&fs, filepath, &s);
    xSemaphoreGive(SettingsReader::access_lock);
    return res == SPIFFS_OK;

}
