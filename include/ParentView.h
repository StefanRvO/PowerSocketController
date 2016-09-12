extern "C"
{
    #include "include/httpd.h"
    #include "include/espfs.h"
    #include "espfs/espfsformat.h"
}

int cgiHook(HttpdConnData *connData);

enum served_state
{
    serving_header,
    serving_base,
    serving_child,
    serving_tail,
};

struct TplData
{
    EspFsFile *file;
    void *tplArg;
    char token[64];
    int tokenPos;
};

struct viewState
{
    served_state state;
    TplData tplatestate;
};


class ParentView
{
    private:
        const char *child_path = "\0";
        viewState *state = nullptr;
    public:
        virtual int ICACHE_FLASH_ATTR child_post(HttpdConnData *connData,  viewState *state);
        virtual int ICACHE_FLASH_ATTR child_get(HttpdConnData *connData, viewState *state);
        virtual int ICACHE_FLASH_ATTR get(HttpdConnData *connData);
        virtual int ICACHE_FLASH_ATTR post(HttpdConnData *connData);
        virtual int ICACHE_FLASH_ATTR doAlloc(viewState * &state, HttpdConnData *connData);
        virtual int ICACHE_FLASH_ATTR serve_tail(HttpdConnData *connData, viewState *state);
        virtual int ICACHE_FLASH_ATTR serve_base(HttpdConnData *connData,  viewState *state);
        virtual int ICACHE_FLASH_ATTR serve_header(HttpdConnData *connData, viewState *state);
        static  int ICACHE_FLASH_ATTR cgiHook(HttpdConnData *connData);
        virtual int ICACHE_FLASH_ATTR ensure_file_open(viewState *state, const char *url);
        virtual void ICACHE_FLASH_ATTR child_callback(HttpdConnData *connData, char *token);
        virtual void ICACHE_FLASH_ATTR parent_callback(HttpdConnData *connData, char *token);
        virtual void ICACHE_FLASH_ATTR parse_and_send_content(HttpdConnData *connData, int len, char *buff, viewState *state, int callback_func);
        void set_child_path(const char * _child_path) { this->child_path = _child_path;}
};
