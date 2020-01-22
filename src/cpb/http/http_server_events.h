#ifndef CPB_HTTP_EVENTS_H
#define CPB_HTTP_EVENTS_H
#include "../cpb_eloop.h"


extern struct cpb_event_handler_itable cpb_event_handler_http_itable;
enum cpb_event_http_cmd {
    CPB_HTTP_INIT, //just initialized with no data
    CPB_HTTP_CONTINUE, //forked in a perisstent connection
    CPB_HTTP_READ, /*ask it to read*/
    CPB_HTTP_SEND, /*ask it to write*/
    CPB_HTTP_DID_READ,  /*inform about async read*/
    CPB_HTTP_DID_WRITE, /*inform about async write*/
    CPB_HTTP_IO_ERROR, /*inform about error during async read/write*/
    CPB_HTTP_CLIENT_CLOSED,
    CPB_HTTP_CLOSE,
};

struct cpb_request_state;
//doesnt add itself to eloop
static int cpb_event_http_init(struct cpb_event *ev, int cmd, struct cpb_request_state *rqstate, int arg) {
    ev->itable = &cpb_event_handler_http_itable;
    ev->msg.u.iip.arg1 = arg;
    ev->msg.u.iip.arg2 = cmd;
    ev->msg.u.iip.argp = rqstate;
    return CPB_OK;
}

#endif// CPB_HTTP_EVENTS_H