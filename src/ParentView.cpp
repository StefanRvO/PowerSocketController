#define ALLOC_OK 50
#ifdef lfscld
#define BASE_PATH "/base.html"
#include "ParentView.h"

#define BASE_CALLBACK  0
#define CHILD_CALLBACK  1

#undef printf

extern "C" {
#include "espressif/esp_common.h"
}

int cgiHook(HttpdConnData *connData)
{
    printf("Entered HOOK\n");
    return HTTPD_CGI_NOTFOUND;

}

int ParentView::child_post(HttpdConnData *connData, viewState *state)
{
    return HTTPD_CGI_NOTFOUND;

}


int ParentView::child_get(HttpdConnData *connData, viewState *state)
{
    if(child_path[0] == '\0') //child path not overwritten, throw not found!
        return HTTPD_CGI_NOTFOUND;
    if(ensure_file_open(state, child_path) == HTTPD_CGI_NOTFOUND)
        return HTTPD_CGI_NOTFOUND;
    char buff[1025];
    int len = espFsRead(state->tplatestate.file, buff, 1024);
    parse_and_send_content(connData, len, buff, state, CHILD_CALLBACK);
    if(len != 1024)
    {   //We are done, close child file and set state to serving_tail
        child_callback(connData, nullptr);
        espFsClose(state->tplatestate.file);
        state->tplatestate.file = nullptr;
        state->state = served_state::serving_tail;
    }
    return HTTPD_CGI_MORE;

}

int ParentView::get(HttpdConnData *connData)
{
    printf("Serving get\n");
    viewState *state = (viewState *)connData->cgiData;
    int alloc_state = doAlloc(state, connData);
    if(alloc_state != ALLOC_OK) return alloc_state;
    if(state->state == served_state::serving_header)
        return serve_header(connData, state);
    if(state->state == served_state::serving_base)
        return serve_base(connData, state);
    else if(state->state == served_state::serving_child)
        return child_get(connData, state);
    else if(state->state == served_state::serving_tail)
        return serve_tail(connData, state);
    else
    {
        return HTTPD_CGI_NOTFOUND;
    }
}

int ParentView::serve_header(HttpdConnData *connData, viewState *state)
{
    httpdStartResponse(connData, 200);
    httpdHeader(connData, "Content-Type", httpdGetMimetype(connData->url));
    httpdEndHeaders(connData);
    state->state = served_state::serving_base;
    return HTTPD_CGI_MORE;
}

int ParentView::serve_base(HttpdConnData *connData, viewState *state)
{
    if(ensure_file_open(state, BASE_PATH) == HTTPD_CGI_NOTFOUND)
        return HTTPD_CGI_NOTFOUND;
    char buff[1025];
    int len = espFsRead(state->tplatestate.file, buff, 1024);
    parse_and_send_content(connData, len, buff, state, BASE_CALLBACK);
    if(len != 1024)
    {   //We are done, close base and set state to serving_child
        parent_callback(connData, nullptr);
        espFsClose(state->tplatestate.file);
        state->tplatestate.file = nullptr;
        state->state = served_state::serving_child;
    }
    return HTTPD_CGI_MORE;
}

void ParentView::parent_callback(HttpdConnData *connData, char *token)
{
    if (token==NULL) return;
    child_callback(connData, token);
}

void ParentView::child_callback(HttpdConnData *connData, char *token)
{
    return;
}

void ParentView::parse_and_send_content(HttpdConnData *connData, int len, char *buff, viewState *state, int callback_func)
{
    TplData &tplstate = state->tplatestate;
    int position = 0;
    char *e = nullptr;
    if (len > 0)
    {
		position = 0;
		e = buff;
		for(int x = 0; x < len; x++)
        {
			if (tplstate.tokenPos == -1)
            {
				//Inside ordinary text.
				if (buff[x] == '%')
                {
					//Send raw data up to now
					if (position != 0) httpdSend(connData, e, position);
					position = 0;
					//Go collect token chars.
					tplstate.tokenPos = 0;
				}
                else
					position++;
			}
            else
            {
				if (buff[x] == '%')
                {
					if (tplstate.tokenPos==0)
                    {
						//This is the second % of a %% escape string.
						//Send a single % and resume with the normal program flow.
						httpdSend(connData, "%", 1);
					}
                    else
                    {
						//This is an actual token.
						tplstate.token[tplstate.tokenPos++] = 0; //zero-terminate token
                        if(callback_func == BASE_CALLBACK)
						    parent_callback(connData, tplstate.token);
                        else if(callback_func == CHILD_CALLBACK)
                            child_callback(connData, tplstate.token);

					}
					//Go collect normal chars again.
					e = &buff[x + 1];
					tplstate.tokenPos=-1;
				}
                else
                {
					if (tplstate.tokenPos < (sizeof(tplstate.token) - 1))
                        tplstate.token[tplstate.tokenPos++] = buff[x];
				}
			}
		}
	}
    //Send remaining bit.
	if (position != 0) httpdSend(connData, e, position);

}

int ParentView::ensure_file_open(viewState *state, const char *url)
{
    if(state->tplatestate.file == nullptr)
    {
        state->tplatestate.file = espFsOpen((char*)url);
        if(state->tplatestate.file == nullptr) return HTTPD_CGI_NOTFOUND;
    }
    return HTTPD_CGI_MORE;
}


int ParentView::serve_tail(HttpdConnData *connData, viewState *state)
{
    httpdSend(connData, "</body></html>", -1);
    delete state;
    return HTTPD_CGI_DONE;
}



int ParentView::post(HttpdConnData *connData)
{
    return HTTPD_CGI_NOTFOUND;
}


int ParentView::cgiHook(HttpdConnData *connData)
{
    printf("Entered Hook in class\n");
    /*ParentView *this_ptr = (ParentView *) connData->cgiArg;
    viewState *state = (viewState *)connData->cgiData;
    if (connData->conn==NULL) {
        //Connection aborted. Clean up.
        this_ptr->parent_callback(connData, (char *)NULL);
        if(state)
        {
            espFsClose(state->tplatestate.file);
            delete state;
        }
        return HTTPD_CGI_DONE;
    }
    if (connData->requestType==HTTPD_METHOD_GET)
        return this_ptr->get(connData);
    if (connData->requestType==HTTPD_METHOD_POST)
        return this_ptr->post(connData);*/
    return HTTPD_CGI_DONE;
}

int ParentView::doAlloc(viewState * &state, HttpdConnData *connData)
{
    if(state == nullptr)
    {
        /*first call*/
        TplData &tplstate = state->tplatestate;
        state = new viewState;
        //memset(state, 0, sizeof(viewState));
        if(state == nullptr) return HTTPD_CGI_NOTFOUND;
        tplstate.file = nullptr;
        tplstate.tplArg = nullptr;
        tplstate.tokenPos = -1;
        if(tplstate.file == nullptr)
        {
            espFsClose(tplstate.file);
            delete state;
            return HTTPD_CGI_DONE;
        }
        if (espFsFlags(tplstate.file) & FLAG_GZIP) {
			//httpd_printf("cgiEspFsTemplate: Trying to use gzip-compressed file %s as template!\n", connData->url);
			espFsClose(tplstate.file);
			delete state;
			return HTTPD_CGI_NOTFOUND;
		}
        connData->cgiData=state;
        state->state = served_state::serving_header;
    }
    return ALLOC_OK;
}
#endif
