extern "C"
{
    #include "FreeRTOS.h"
    #include "semphr.h"
}

#define WIFI_SSID_FILE "wifi_ssid"
#define WIFI_PASSWORD_FILE "wifi_password"

#define ID_FILE "id"
#define NAME_FILE "name"

class SettingsReader
{
    private:
        static bool initialised;
    public:
        static xSemaphoreHandle access_lock;
        static bool init();
        static bool readfile(const char *filepath, char *buf, uint16_t max_len);
        static bool writefile(const char *filepath, char *buf, uint16_t write_len);
        static bool file_exists(const char *filepath);
};
