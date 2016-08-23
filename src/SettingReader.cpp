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

    return true;
}


bool SettingsReader::readfile(const char *filepath, char *buf, uint16_t max_len, bool binary)
{
    if(!SettingsReader::file_exists(filepath)) return false;
    xSemaphoreTake(SettingsReader::access_lock, portMAX_DELAY);
    int fd = SPIFFS_open(&fs, filepath, SPIFFS_RDONLY, 0);
    int read_bytes = SPIFFS_read(&fs, fd, buf, max_len);

    if(read_bytes == SPIFFS_ERR_END_OF_OBJECT) read_bytes = 0;

    if(!binary)
        buf[read_bytes] = '\0';
    //printf("%s\n", buf);
    SPIFFS_close(&fs, fd);
    xSemaphoreGive(SettingsReader::access_lock);
    return true;
}


bool SettingsReader::writefile(const char *filepath, char *buf, uint16_t write_len)
{
    xSemaphoreTake(SettingsReader::access_lock, portMAX_DELAY);
    int fd = SPIFFS_open(&fs, filepath, SPIFFS_TRUNC | SPIFFS_CREAT | SPIFFS_WRONLY, 0);
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

uint32_t SettingsReader::get_id(bool *success)
{
    uint32_t tmp_id;
    bool got_id = SettingsReader::readfile(ID_FILE, (char *)&tmp_id, sizeof(tmp_id), true);
    if(success != nullptr) *success = got_id;
    return tmp_id;
}

bool SettingsReader::set_id(uint32_t id)
{
    return SettingsReader::writefile(ID_FILE, (char *)&id, sizeof(id));
}

bool SettingsReader::get_switch_state(uint8_t switch_number)
{
    uint8_t state;
    char file_name[sizeof(SWITCH_FILE_PREFIX) + 10];
    sprintf(file_name, SWITCH_FILE_PREFIX "%d", state);
    bool got_state = SettingsReader::readfile(file_name, (char *)&state, sizeof(state), true);
    if(!got_state) return false; //default to off
    return state;
}

bool SettingsReader::set_switch_state(uint8_t switch_number, bool state)
{
    char file_name[sizeof(SWITCH_FILE_PREFIX) + 10];
    sprintf(file_name, SWITCH_FILE_PREFIX "%d", state);
    return SettingsReader::writefile(file_name, (char *)&state, sizeof(state));
}
