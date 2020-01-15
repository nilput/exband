#ifndef CPB_HTTP_EVENTS_H
#define CPB_HTTP_EVENTS_H
#include "../cpb_eloop.h"


extern struct cpb_event_handler_itable cpb_event_handler_http_itable;
enum cpb_event_http_cmd {
    CPB_HTTP_INIT, //just initialized with no data
    CPB_HTTP_CONTINUE, //forked in a perisstent connection
    CPB_HTTP_READ,
    CPB_HTTP_SEND,
    CPB_HTTP_CLOSE,
};

struct cpb_request_state;
//doesnt add itself to eloop
static int cpb_event_http_init(struct cpb_event *ev, int socket_fd, int cmd, struct cpb_request_state *rqstate) {
    ev->itable = &cpb_event_handler_http_itable;
    ev->msg.arg1 = socket_fd;
    ev->msg.arg2 = cmd;
    ev->msg.argp = rqstate;
    return CPB_OK;
}

#endif// CPB_HTTP_EVENTS_H