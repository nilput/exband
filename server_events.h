#include "eloop.h"

extern struct cpb_event_handler_itable cpb_event_handler_http_itable;
enum cpb_event_http_cmd {
    CPB_HTTP_INIT,
    CPB_HTTP_READ,
    CPB_HTTP_SEND,
    CPB_HTTP_HANDLE_HEADERS,
    CPB_HTTP_HANDLE_BODY,
    CPB_HTTP_CLOSE,
};

//doesnt add itself to eloop
static int cpb_event_http_init(struct cpb_event *ev, int socket_fd, int cmd, struct cpb_request_state *rstate) {
    ev->itable = &cpb_event_handler_http_itable;
    ev->msg.arg1 = socket_fd;
    ev->msg.arg2 = cmd;
    ev->msg.argp = rstate;
    return CPB_OK;
}