#pragma once
//Handles logins to the webserver
//Save sha256 hashed passwords. The passwords are salted with 64 random bits

//The hash and salt are saved to nvs storage. the username is the key to the nvs, and thus also
//Have a max length of 15.
#define MAX_SESSIONS 10
class LoginManager;

enum user_type: uint8_t
{
    admin = 0;
}

struct Session
{
    uint128_t session_id; //Session id for this instance. Must be unique
    char username[16];
    user_type u_type; //Access control for the user.
    double created; //Time where this session was created
    double last_used; //Time when the session was last used
}

class LoginManager
{
    public:
        static LoginManager *get_instance();
        static Session sessions[10]; //Max ten active sessions at a time
        ~LoginManager();
        void add_user(char *username, char *passwd);
        void remove_user(char *username);
        void change_passwd(char *username, char *oldpasswd, char *newpasswd);
        Session *get_session(uint128_t session_id);
        Session *perform_login(char *username, char *passwd);
    private:
        static LoginManager *instance;
        LoginManager();
        SemaphoreHandle_t session_lock; //Lock for modifying the sessions
        SemaphoreHandle_t user_lock; //Lock for modifying the users


};
