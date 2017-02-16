extern "C"
{
    #include "esp_partition.h"
    #include "nvs_flash.h"
    #include <stdio.h>
    #include "esp_wifi.h" //For wifi default values

}

#include "SettingsHandler.h"

__attribute__((unused)) static const char *TAG = "SettingsHandler";

SettingsHandler *SettingsHandler::instance = nullptr;

SettingsHandler *SettingsHandler::get_instance()
{
    if(SettingsHandler::instance == nullptr)
    {
        SettingsHandler::instance = new SettingsHandler();
    }
    return SettingsHandler::instance;
}

SettingsHandler::SettingsHandler()
{
    nvs_flash_init();
    //Open nvs for read and write
    ESP_ERROR_CHECK( nvs_open("PWRSCK_SETTINGS", NVS_READWRITE, &this->handle) );
    this->set_default_values();

}

void SettingsHandler::set_default_values()
{
    //We set up all the default values into the NVS store if they
    //Do not exist already. If they do exists, we won't overwrite them
    ESP_ERROR_CHECK( set_default_value("STA_SSID","TEST") );
    ESP_ERROR_CHECK( set_default_value("STA_PASSWORD","") );
    ESP_ERROR_CHECK( set_default_value("AP_SSID", "ESP_AP_32"));
    ESP_ERROR_CHECK( set_default_value("AP_PASS", "PASSWORD"));
    ESP_ERROR_CHECK( set_default_value("AP_AUTH", (uint32_t)WIFI_AUTH_OPEN));

}

void SettingsHandler::reset_settings()
{
    ESP_ERROR_CHECK( nvs_erase_all(this->handle) );
    ESP_ERROR_CHECK( nvs_commit(this->handle)    );
    this->set_default_values();
}

esp_err_t SettingsHandler::nvs_get(const char *key, int8_t *val)
{ return nvs_get_i8(this->handle, key, val); }

esp_err_t SettingsHandler::nvs_get(const char *key, uint8_t *val)
{ return nvs_get_u8(this->handle, key, val); }

esp_err_t SettingsHandler::nvs_get(const char *key, int16_t *val)
{ return nvs_get_i16(this->handle, key, val); }

esp_err_t SettingsHandler::nvs_get(const char *key, uint16_t *val)
{ return nvs_get_u16(this->handle, key, val); }

esp_err_t SettingsHandler::nvs_get(const char *key, int32_t *val)
{ return nvs_get_i32(this->handle, key, val); }

esp_err_t SettingsHandler::nvs_get(const char *key, uint32_t *val)
{ return nvs_get_u32(this->handle, key, val); }

esp_err_t SettingsHandler::nvs_get(const char *key, int64_t *val)
{ return nvs_get_i64(this->handle, key, val); }

esp_err_t SettingsHandler::nvs_get(const char *key, uint64_t *val)
{ return nvs_get_u64(this->handle, key, val); }

esp_err_t SettingsHandler::nvs_get(const char *key, const char *buff, size_t *len)
{ return nvs_get_str(this->handle, key, (char *)buff, len); }


esp_err_t SettingsHandler::nvs_get(const char *key, void *buff, size_t *len)
{ return nvs_get_blob(this->handle, key, buff, len); }


esp_err_t SettingsHandler::nvs_set(const char *key, int8_t val)
{
    esp_err_t err = nvs_set_i8(this->handle, key, val);
    nvs_commit(this->handle);
    return err;
}

esp_err_t SettingsHandler::nvs_set(const char *key, uint8_t val)
{
    esp_err_t err = nvs_set_u8(this->handle, key, val);
    nvs_commit(this->handle);
    return err;
}

esp_err_t SettingsHandler::nvs_set(const char *key, int16_t val)
{
    esp_err_t err = nvs_set_i16(this->handle, key, val);
    nvs_commit(this->handle);
    return err;
}

esp_err_t SettingsHandler::nvs_set(const char *key, uint16_t val)
{
    esp_err_t err = nvs_set_u16(this->handle, key, val);
    nvs_commit(this->handle);
    return err;
}

esp_err_t SettingsHandler::nvs_set(const char *key, int32_t val)
{
    esp_err_t err = nvs_set_i32(this->handle, key, val);
    nvs_commit(this->handle);
    return err;
}

esp_err_t SettingsHandler::nvs_set(const char *key, uint32_t val)
{
    esp_err_t err = nvs_set_u32(this->handle, key, val);
    nvs_commit(this->handle);
    return err;
}

esp_err_t SettingsHandler::nvs_set(const char *key, int64_t val)
{
    esp_err_t err = nvs_set_i64(this->handle, key, val);
    nvs_commit(this->handle);
    return err;
}

esp_err_t SettingsHandler::nvs_set(const char *key, uint64_t val)
{
    esp_err_t err = nvs_set_u64(this->handle, key, val);
    nvs_commit(this->handle);
    return err;
}

esp_err_t SettingsHandler::nvs_set(const char *key, const char *buff)
{
    esp_err_t err = nvs_set_str(this->handle, key, buff);
    nvs_commit(this->handle);
    return err;
}

esp_err_t SettingsHandler::nvs_set(const char *key, const void *buff, size_t len)
{
    esp_err_t err = nvs_set_blob(this->handle, key, buff, len);
    nvs_commit(this->handle);
    return err;
}
