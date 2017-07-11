#include "LoginManager.h"
#include <cstring>
#include "esp_system.h"
#include <mbedtls/pkcs5.h>
#include "esp_log.h"
#include <algorithm>
__attribute__((unused)) static const char *TAG = "LoginManager";

LoginManager *LoginManager::instance = nullptr;
Session LoginManager::sessions[MAX_SESSIONS];

#define DEFAULT_USER "admin"
#define DEFAULT_USER_PASSWD "password"

Session::Session(const char *_username, user_type _type, uint64_t current_time)
:valid(true), created(current_time), last_used(current_time), u_type(_type)
{
    strncpy(this->username, _username, MAX_USERNAMELEN);
    this->username[MAX_USERNAMELEN - 1] = '\0';
    *( ((uint32_t *)this->session_id.key) + 0 ) = esp_random();
    *( ((uint32_t *)this->session_id.key) + 1 ) = esp_random();
    *( ((uint32_t *)this->session_id.key) + 2 ) = esp_random();
    *( ((uint32_t *)this->session_id.key) + 3 ) = esp_random();
};


#include <libwebsockets.h>
void LoginManager::print_session(Session *session)
{
    ESP_LOGI(TAG, "Username:\t%.*s\n", MAX_USERNAMELEN, session->username );
    ESP_LOGI(TAG, "Type:\t%d\n", session->u_type);
    ESP_LOGI(TAG, "Created:\t%llu\n", session->created);
    ESP_LOGI(TAG, "Last Use:\t%llu\n", session->last_used);
    ESP_LOGI(TAG, "Valid:\t%d\n", session->valid);
    this->print_session_key(&session->session_id);
}
void LoginManager::print_session_key(session_key *session_id)
{
    char session_id_b64[SESSION_ID_ENCODED_BUF_LEN];
    lws_b64_encode_string((const char*)session_id, sizeof(session_key), session_id_b64, sizeof(session_id_b64));
    ESP_LOGI(TAG, "Session id:\t%s\n", session_id_b64);
}

void LoginManager::print_all_sessions()
{
    for(uint8_t i = 0; i < MAX_SESSIONS; i++)
    {
            xSemaphoreTake(this->session_lock, 100000 / portTICK_RATE_MS);
            print_session(this->sessions + i);
            xSemaphoreGive(this->session_lock);
    }
}

LoginManager *LoginManager::get_instance()
{
    if(LoginManager::instance == nullptr)
    {
        LoginManager::instance = new LoginManager();
    }
    return LoginManager::instance;

}

Session *LoginManager::get_session(const char *username)
{
    for(uint8_t i = 0; i < MAX_SESSIONS; i++)
    {
        if(this->sessions[i].valid && strncmp(username, this->sessions[i].username, MAX_USERNAMELEN) == 0)
        {
            return this->sessions + i;
        }
    }
    return nullptr;
}


Session *LoginManager::get_session(session_key *session)
{
    for(uint8_t i = 0; i < MAX_SESSIONS; i++)
    {
        if(this->sessions[i].valid == true && memcmp(session, &this->sessions[i].session_id, sizeof(session_key)) == 0)
        {
            return this->sessions + i;
        }
    }
    return nullptr;
}

login_error LoginManager::remove_user(const char *username)
{
    size_t len = 0;
    esp_err_t err = nvs_get_blob(this->nvs_login_handle, username, (char *)nullptr, &len);
    if(err == ESP_OK)
    {
        nvs_erase_key(this->nvs_login_handle, username);
        this->logout(username);
        return no_error;
    }
    return user_do_not_exist;
}

login_error LoginManager::logout(const char *username)
{
    bool found = false;
    xSemaphoreTake(this->session_lock, 100000 / portTICK_RATE_MS);
    Session *session = this->get_session(username);
    if(session != nullptr)
    {
        session->valid = false;
        found = true;
    }
    xSemaphoreGive(this->session_lock);
    if(!found) return session_invalid;
    return no_error;
}

login_error LoginManager::get_user_type(session_key *session_id, user_type *type)
{
    bool found = false;
    xSemaphoreTake(this->session_lock, 100000 / portTICK_RATE_MS);
    Session *session = this->get_session(session_id);
    if(session != nullptr)
    {
        *type = session->u_type;
        found = true;
    }
    else *type = no_access;
    xSemaphoreGive(this->session_lock);
    if(!found) return session_invalid;
    return no_error;
}

login_error LoginManager::get_session_info(session_key *session_id, Session *dest)
{
    bool found = false;
    xSemaphoreTake(this->session_lock, 100000 / portTICK_RATE_MS);
    Session *session = this->get_session(session_id);
    if(session != nullptr)
    {
        memcpy(dest, session, sizeof(Session));
        found = true;
    }
    xSemaphoreGive(this->session_lock);
    if(!found) return session_invalid;
    return no_error;
}



login_error LoginManager::get_username(const char *username, session_key *session_id)
{
    bool found = false;
    xSemaphoreTake(this->session_lock, 100000 / portTICK_RATE_MS);
    Session *session = this->get_session(session_id);
    if(session != nullptr)
    {
        memcpy((void *)username, (void *)session->username, MAX_USERNAMELEN);
        found = true;
    }
    xSemaphoreGive(this->session_lock);
    if(found) return no_error;
    return session_invalid;
}

login_error LoginManager::perform_login(const char *username, const char *passwd, session_key *session, bool create_session)
{
    size_t len = 0;
    if(!username or !passwd or !session) return fatal_error;
    esp_err_t err = nvs_get_blob(this->nvs_login_handle, username, (char *)nullptr, &len);
    if(err != ESP_OK) return invalid_username;
    User loaded_user;
    uint8_t hash[sizeof(loaded_user.hash)];
    len = sizeof(User);
    err = nvs_get_blob(this->nvs_login_handle, username, (char *)&loaded_user, &len);
    if(err != ESP_OK)
        return fatal_error;
    this->get_hash(passwd, loaded_user.salt, hash, sizeof(User::hash));
    int cmp_result = memcmp(hash, loaded_user.hash, sizeof(User::hash));
    if(cmp_result != 0) return invalid_password;
    if(create_session == false) return no_error;
    return this->create_session(username, admin, session, this->time_keeper->get_uptime_milliseconds());
}

login_error LoginManager::change_passwd(session_key *session_id, char *old_pass, char *new_pass)
{
    bool found = false;
    xSemaphoreTake(this->session_lock, 100000 / portTICK_RATE_MS);
    Session *session = this->get_session(session_id);
    login_error error = no_error;
    while(session) //We do this in a while loop so we can use break.. Kind of hacky but whatever..
    {
        found = true;
        //Verify current password
        size_t len = 0;
        if(!session->username or !old_pass or !session) return fatal_error;
        esp_err_t err = nvs_get_blob(this->nvs_login_handle, session->username, (char *)nullptr, &len);
        if(err != ESP_OK)
        {
            error = invalid_username;
            break;
        }
        User loaded_user;
        uint8_t hash[sizeof(loaded_user.hash)];
        len = sizeof(User);
        err = nvs_get_blob(this->nvs_login_handle, session->username, (char *)&loaded_user, &len);
        if(err != ESP_OK)
        {
            error = fatal_error;
            break;
        }
        this->get_hash(old_pass, loaded_user.salt, hash, sizeof(User::hash));
        int cmp_result = memcmp(hash, loaded_user.hash, sizeof(User::hash));
        if(cmp_result != 0)
        {
            error = invalid_password;
            break;
        }

        loaded_user.salt = this->generate_salt();
        this->get_hash(new_pass, loaded_user.salt, loaded_user.hash, sizeof(User::hash));
        ESP_ERROR_CHECK( nvs_set_blob(this->nvs_login_handle, session->username, (uint8_t *)&loaded_user, sizeof(User)) );
        ESP_ERROR_CHECK( nvs_commit(this->nvs_login_handle));
        break;
    }

    xSemaphoreGive(this->session_lock);
    if(found) return error;
    return session_invalid;
}

login_error LoginManager::is_valid(session_key *session_id)
{
    bool valid = false;
    xSemaphoreTake(this->session_lock, 100000 / portTICK_RATE_MS);
    Session *session = this->get_session(session_id);
    if(session)
    {
        this->update_session(session, this->time_keeper->get_uptime_milliseconds());
        valid = session->valid;
    }
    xSemaphoreGive(this->session_lock);
    if(valid) return no_error;
    return session_invalid;
}

login_error LoginManager::logout(session_key *session_id)
{
    bool found = false;
    //this->print_all_sessions();
    xSemaphoreTake(this->session_lock, 100000 / portTICK_RATE_MS);
    Session *session = this->get_session(session_id);
    //ESP_LOGI(TAG, "%p\n", session);
    this->print_session_key(session_id);

    if(session != nullptr)
    {
        session->valid = false;
        found = true;
    }
    xSemaphoreGive(this->session_lock);
    //this->print_all_sessions();
    if(!found) return session_invalid;
    return no_error;
}

LoginManager::LoginManager()
:time_keeper(TimeKeeper::get_instance()), s_handler(SettingsHandler::get_instance())
{
    this->session_lock = xSemaphoreCreateMutex();
    ESP_ERROR_CHECK( nvs_open("LOGINMANGER", NVS_READWRITE, &this->nvs_login_handle) );
    //Add the default user if this is first boot
    uint8_t mngr_state;
    ESP_ERROR_CHECK( this->s_handler->nvs_get("LGN_MNGR_STATE", &mngr_state));
    if(!mngr_state)
    {
        ESP_ERROR_CHECK( nvs_erase_all(this->nvs_login_handle) );
        ESP_ERROR_CHECK( nvs_commit(this->nvs_login_handle));
        ESP_ERROR_CHECK( this->s_handler->nvs_set("LGN_MNGR_STATE", (uint8_t)1));
        this->add_user(DEFAULT_USER, DEFAULT_USER_PASSWD);
    }
}

login_error LoginManager::add_user(const char *username, const char *password)
{
    User new_user;
    size_t len = 0;
    esp_err_t err = nvs_get_blob(this->nvs_login_handle, username, (char *)nullptr, &len);
    if(err == ESP_ERR_NVS_INVALID_NAME || strlen(username) > MAX_USERNAMELEN - 1) return login_error::invalid_username;
    if(err == ESP_OK) return login_error::user_already_exist;
    printf("ERROR: %d\n", err);
    new_user.salt = this->generate_salt();
    this->get_hash(password, new_user.salt, new_user.hash, sizeof(User::hash));
    ESP_ERROR_CHECK( nvs_set_blob(this->nvs_login_handle, username, (uint8_t *)&new_user, sizeof(User)) );
    ESP_ERROR_CHECK( nvs_commit(this->nvs_login_handle));
    return no_error;
}

login_error LoginManager::create_session(const char *username, user_type type, session_key *session, uint64_t _time)
{
    //Find the session to replace
    xSemaphoreTake(this->session_lock, 100000 / portTICK_RATE_MS);

    Session *oldest = this->sessions + 0;
    Session *same_username = nullptr;
    Session *invalid = nullptr;
    for(uint8_t i = 0; i < MAX_SESSIONS; i++)
    {
        this->update_session(this->sessions + i, _time);
        if(this->sessions[i].valid == false) invalid = &this->sessions[i];
        if(this->sessions[i].last_used < oldest->last_used) oldest = &this->sessions[i];
        if( strncmp(this->sessions[i].username, username, MAX_USERNAMELEN) == 0) same_username = &this->sessions[i];
    }
    if(same_username)
    {
        *same_username = Session(username, type, _time);
        *session = same_username->session_id;
    }
    else if(invalid)
    {
        *invalid = Session(username, type, _time);
        *session = invalid->session_id;
    }
    else if(oldest)
    {
        *oldest = Session(username, type, _time);
        *session = oldest->session_id;
    }
    xSemaphoreGive(this->session_lock);
    return no_error;
}

void LoginManager::update_session(Session *session, uint64_t cur_time)
{
    if(session->valid)
    {
        session->valid = this->get_expire_time(session) > cur_time;
        session->last_used = this->time_keeper->get_uptime_milliseconds();
    }
}

uint64_t LoginManager::get_expire_time(Session *session)
{
    uint64_t expire_inactive = session->last_used + EXPIRE_INACTIVE;
    uint64_t expire_max = session->last_used + EXPIRE_MAX;
    return std::min(expire_inactive, expire_max);
}

uint64_t LoginManager::LoginManager::generate_salt()
{
    uint64_t salt = esp_random(); /* Uses hardware RNG. */
    salt |=  ((uint64_t)esp_random()) << 32;
    return salt;
}

void LoginManager::get_hash(const char *password, uint64_t salt, uint8_t *hash_buff, size_t hash_len)
{
    size_t passwd_len = strlen(password);
    const mbedtls_md_info_t *info_sha1;
    mbedtls_md_context_t sha1_ctx;
    int ret;

    mbedtls_md_init( &sha1_ctx );
    info_sha1 = mbedtls_md_info_from_type( MBEDTLS_MD_SHA1 );
    if( info_sha1 == NULL )
    {
        ESP_LOGI(TAG, "ERROR initialising sha1 context");
        return;
    }
    if( ( ret = mbedtls_md_setup( &sha1_ctx, info_sha1, 1 ) ) != 0 )
    {
        ESP_LOGI(TAG, "ERROR initialising sha1 context");
        return;
    }
    ret = mbedtls_pkcs5_pbkdf2_hmac( &sha1_ctx, (uint8_t*)password, passwd_len, (uint8_t*)&salt,
                              sizeof(salt), 500, hash_len, hash_buff );
    if( ret != 0)
    {
        ESP_LOGI(TAG, "ERROR computing PBKDF2 sum");
    }

    mbedtls_md_free( &sha1_ctx );
}
