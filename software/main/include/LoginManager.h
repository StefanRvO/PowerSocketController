#pragma once
//Handles logins to the webserver
//Save pbkdf2 hashed passwords. The passwords are salted with 64 random bits

//The hash and salt are saved to nvs storage. the username is the key to the nvs, and thus also
//Have a max length of 15.
extern "C"
{
    #include "nvs.h"
    #include "freertos/FreeRTOS.h"

    #include "freertos/task.h"
    #include "freertos/semphr.h"

}
#include "TimeKeeper.h"

#include <cstdint>
typedef struct session_key {uint8_t key[16];} session_key;
#define MAX_USERNAMELEN 16
#define MAX_SESSIONS 10
#define EXPIRE_MAX ((uint64_t)60 * (uint64_t)60 * (uint64_t)24 * (uint64_t)31 * (uint64_t)1000) //1 month
#define EXPIRE_INACTIVE ((uint64_t)60 * (uint64_t)60 * (uint64_t)24 * (uint64_t)7) //1 week
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
    no_access = 0,
    admin
};

struct User
{
    uint64_t salt;
    uint8_t hash[256 / 8];
    user_type type;
};

struct Session
{
    Session(char *_username, user_type _type, uint64_t current_time);
    Session() {};
    session_key session_id; //Session id for this instance. Must be unique
    char username[MAX_USERNAMELEN];
    user_type u_type; //Access control for the user.
    uint64_t created; //Time where this session was created
    uint64_t last_used; //Time when the session was last used
    bool valid = false;
};

class LoginManager
{
    public:
        static LoginManager *get_instance();
        ~LoginManager() {};
        login_error add_user(char *username, char *passwd); //done
        login_error remove_user(char *username); //done
        login_error change_passwd(char *username, char *oldpasswd, char *newpasswd);
        login_error get_username(char *username, session_key *session); //done
        login_error perform_login(char *username, char *passwd, session_key *session); //done
        login_error logout(char *username); //done
        login_error logout(session_key *session); //done
        login_error is_valid(session_key *session); //done

    private:
        static Session sessions[MAX_SESSIONS]; //Max ten active sessions at a time
        login_error create_session(char *username, user_type type, session_key *session, uint64_t);
        Session *get_session(char *username);
        Session *get_session(session_key *session);
        uint64_t get_expire_time(Session *session);
        void update_session(Session *session, uint64_t cur_time);
        TimeKeeper *time_keeper;
        static LoginManager *instance;
        LoginManager();
        nvs_handle nvs_login_handle;
        SemaphoreHandle_t session_lock; //Lock for modifying the sessions
        uint64_t generate_salt();
        void get_hash(char *password, uint64_t salt, uint8_t *hash_buff, size_t hash_len);
};
