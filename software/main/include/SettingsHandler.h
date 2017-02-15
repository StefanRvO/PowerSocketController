#pragma once

//Class for saving and retreiving settings, primarily for storage in nvs.

extern "C"
{
    #include "nvs.h"
}

class SettingsHandler;

class SettingsHandler
{
    public:
        static SettingsHandler *get_instance();
        ~SettingsHandler();
        nvs_handle handle;
        void set_default_values();
        void reset_settings();
        void set_default_value();

        esp_err_t nvs_get(const char *key, int8_t *val);
        esp_err_t nvs_get(const char *key, uint8_t *val);
        esp_err_t nvs_get(const char *key, int16_t *val);
        esp_err_t nvs_get(const char *key, uint16_t *val);
        esp_err_t nvs_get(const char *key, int32_t *val);
        esp_err_t nvs_get(const char *key, uint32_t *val);
        esp_err_t nvs_get(const char *key, int64_t *val);
        esp_err_t nvs_get(const char *key, uint64_t *val);
        esp_err_t nvs_get(const char *key, const char *buff, size_t *len);
        esp_err_t nvs_get(const char *key, void *buff, size_t *len);

        esp_err_t nvs_set(const char *key, int8_t val);
        esp_err_t nvs_set(const char *key, uint8_t val);
        esp_err_t nvs_set(const char *key, int16_t val);
        esp_err_t nvs_set(const char *key, uint16_t val);
        esp_err_t nvs_set(const char *key, int32_t val);
        esp_err_t nvs_set(const char *key, uint32_t val);
        esp_err_t nvs_set(const char *key, int64_t val);
        esp_err_t nvs_set(const char *key, uint64_t val);
        esp_err_t nvs_set(const char *key, const char *buff);
        esp_err_t nvs_set(const char *key, const void *buff, size_t len);

    private:
        static SettingsHandler *instance;
        SettingsHandler();

        template <class T>
        esp_err_t set_default_value(const char *key, T val)
        {
            T tmp_val;
            esp_err_t err = nvs_get(key, &tmp_val);
            if(err == ESP_OK)
                return err;
            return this->nvs_set(key, val);
        }

        esp_err_t set_default_value(const char *key, const char * buff)
        {
            size_t len = 0;
            esp_err_t err = nvs_get(key, (char *)nullptr, &len);
            if(err == ESP_OK)
                return err;
            return this->nvs_set(key, buff);
        }
        template <class T>
        esp_err_t set_default_value(const char *key, T val, size_t len)
        {
            size_t len_tmp = len;
            esp_err_t err = nvs_get(key, (T) nullptr, &len_tmp);
            if(err == ESP_OK || err == ESP_ERR_NVS_INVALID_LENGTH)
                return ESP_OK;
            return this->nvs_set(key, val, len);
        }

};
