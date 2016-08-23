extern "C"
{
    #include "FreeRTOS.h"
    #include "semphr.h"
}

#define WIFI_SSID_FILE "wifi_ssid"
#define WIFI_PASSWORD_FILE "wifi_password"

#define ID_FILE "id"
#define NAME_FILE "name"

#define SWITCH_FILE_PREFIX "switch_state"


class SettingsReader
{
    private:
        static bool initialised;
        static bool readfile(const char *filepath, char *buf, uint16_t max_len, bool binary = false);
        static bool writefile(const char *filepath, char *buf, uint16_t write_len);
        static bool file_exists(const char *filepath);
    public:
        static xSemaphoreHandle access_lock;
        static bool init();
        static uint32_t get_id(bool *success = nullptr);
        static bool get_name(char *buf, uint16_t max_size) { return readfile(NAME_FILE, buf, max_size); }
        static bool get_wifi_ssid(char *buf, uint16_t max_size) { return readfile(WIFI_SSID_FILE, buf, max_size); }
        static bool get_wifi_password(char *buf, uint16_t max_size) { return readfile(WIFI_PASSWORD_FILE, buf, max_size); }

        static bool set_id(uint32_t id);
        static bool set_name(char *buf, uint16_t len) { return writefile(NAME_FILE, buf, len); }
        static bool set_wifi_ssid(char *buf, uint16_t len) { return writefile(WIFI_SSID_FILE, buf, len); }
        static bool set_wifi_password(char *buf, uint16_t len) { return writefile(WIFI_PASSWORD_FILE, buf, len); }
        static bool get_switch_state(uint8_t switch_number);
        static bool set_switch_state(uint8_t switch_number, bool state);
};
