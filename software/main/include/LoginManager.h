#pragma once
//Handles logins to the webserver
//Save sha256 hashed passwords. The passwords are salted with 64 random bits

//The hash and salt are saved to nvs storage. the username is the key to the nvs, and thus also
//Have a max length of 15.
extern "C"
{
    #include "nvs.h"
    #include "freertos/FreeRTOS.h"

    #include "freertos/task.h"
    #include "freertos/semphr.h"

}
#include <cstdint>
typedef struct session_key {uint8_t key[16];} session_key;
#define MAX_SESSIONS 10
class LoginManager;

enum login_error
{
    no_error = 0,
    user_already_exist,
    user_do_not_exist,
    invalid_password,
    invalid_username,
    fatal_error,
    session_invalid,
};

enum user_type: uint8_t
{
    admin = 0,
};

struct User
{
    uint64_t salt;
    uint8_t hash[256 / 8];
    user_type type;
};
struct Session
{
    session_key session_id; //Session id for this instance. Must be unique
    char username[16];
    user_type u_type; //Access control for the user.
    double created; //Time where this session was created
    double last_used; //Time when the session was last used
};

class LoginManager
{
    public:
        static LoginManager *get_instance();
        static Session sessions[10]; //Max ten active sessions at a time
        ~LoginManager() {};
        login_error add_user(char *username, char *passwd);
        login_error remove_user(char *username);
        login_error change_passwd(char *username, char *oldpasswd, char *newpasswd);
        login_error get_username( char *username, session_key *session);
        login_error perform_login(char *username, char *passwd, session_key *session);
    private:
        static LoginManager *instance;
        LoginManager();
        nvs_handle nvs_login_handle;
        SemaphoreHandle_t session_lock; //Lock for modifying the sessions
        SemaphoreHandle_t user_lock; //Lock for modifying the users
        uint64_t generate_salt();
        void get_hash(char *password, uint64_t salt, uint8_t *hash_buff);
};
