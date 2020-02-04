#ifndef CPB_HTTP_EVENTS_H
#define CPB_HTTP_EVENTS_H
#include "../cpb_eloop.h"


extern struct cpb_event_handler_itable cpb_event_handler_http_itable;
enum cpb_event_http_cmd {
    CPB_HTTP_READ, /*ask it to read, .argp : rqstate*/
    CPB_HTTP_SEND, /*ask it to write, .argp : rqstate*/
    CPB_HTTP_DID_READ,  /*inform about async read result, .argp : rqstate*/
    CPB_HTTP_DID_WRITE, /*inform about async write result, .argp : rqstate*/
    CPB_HTTP_WRITE_IO_ERROR, /*inform about error during async read/write, .argp : rqstate*/
    CPB_HTTP_READ_IO_ERROR,
    CPB_HTTP_INPUT_BUFFER_FULL, /*inform about a full buffer during async read, .argp : rqstate*/
    CPB_HTTP_CLIENT_CLOSED, //.argp : rqstate
    CPB_HTTP_CANCEL, /*.argp: http server, .arg1: socket*/
};

struct cpb_request_state;
//doesnt add itself to eloop
static int cpb_event_http_init(struct cpb_event *ev, int cmd, void *object, int arg) {
    ev->itable = &cpb_event_handler_http_itable;
    ev->msg.u.iip.arg1 = arg;
    ev->msg.u.iip.arg2 = cmd;
    ev->msg.u.iip.argp = object;
    return CPB_OK;
}
int cpb_handle_http_event(struct cpb_event ev);

#endif// CPB_HTTP_EVENTS_H