#include "LoginManager.h"
#include <cstring>
#include "esp_system.h"
#include <mbedtls/sha256.h>

__attribute__((unused)) static const char *TAG = "LoginManager";

LoginManager *LoginManager::instance = nullptr;


LoginManager *LoginManager::get_instance()
{
    if(LoginManager::instance == nullptr)
    {
        LoginManager::instance = new LoginManager();
    }
    return LoginManager::instance;

}

LoginManager::LoginManager()
{
    ESP_ERROR_CHECK( nvs_open("LOGINMANGER", NVS_READWRITE, &this->nvs_login_handle) );
}

login_error LoginManager::add_user(char *username, char *password)
{
    size_t len = 0;
    esp_err_t err = nvs_get_blob(this->nvs_login_handle, username, (char *)nullptr, &len);
    if(err == ESP_OK) return login_error::user_already_exist;
    User *new_user = (User *)malloc(sizeof(User));
    new_user->salt = this->generate_salt();
    this->get_hash(password, new_user->salt, new_user->hash);
    ESP_ERROR_CHECK( nvs_set_blob(this->nvs_login_handle, username, (uint8_t *)new_user, sizeof(new_user)) );
    return no_error;
}

uint64_t LoginManager::LoginManager::generate_salt()
{
    uint64_t salt = esp_random(); /* Uses hardware RNG. */
    salt |=  ((uint64_t)esp_random()) << 32;
    return salt;
}

void LoginManager::get_hash(char *password, uint64_t salt, uint8_t *hash_buff)
{
    size_t passwd_len = strlen(password);
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, (uint8_t *)password, passwd_len);
    mbedtls_sha256_update(&ctx, (uint8_t *)&salt, sizeof(salt));
    mbedtls_sha256_finish(&ctx, hash_buff);
    mbedtls_sha256_free(&ctx);
}
