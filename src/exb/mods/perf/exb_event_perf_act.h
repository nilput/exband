#ifndef EXB_PERF_ACT_H
#define EXB_PERF_ACT_H
#include "../../exb_eloop.h"

struct exb_perf_act_state {
    struct exb_eloop *eloop; //not owned, must outlive
};

enum exb_event_perf_act_cmd {
    EXB_PERF_ACT_INIT, //just initialized with no data, .argp : rqstate
    EXB_PERF_ACT_CONTINUE, //forked in a persistent connection, .argp : rqstate
    EXB_PERF_ACT_READ, /*ask it to read, .argp : rqstate*/
    EXB_PERF_ACT_SEND, /*ask it to write, .argp : rqstate*/
    EXB_PERF_ACT_DID_READ,  /*inform about async read result, .argp : rqstate*/
    EXB_PERF_ACT_DID_WRITE, /*inform about async write result, .argp : rqstate*/
    EXB_PERF_ACT_WRITE_IO_ERROR, /*inform about error during async read/write, .argp : rqstate*/
    EXB_PERF_ACT_READ_IO_ERROR,
    EXB_PERF_ACT_INPUT_BUFFER_FULL, /*inform about a full buffer during async read, .argp : rqstate*/
    EXB_PERF_ACT_CLIENT_CLOSED, //.argp : rqstate
    EXB_PERF_ACT_CANCEL, /*.argp: http server, .arg1: socket*/
};

struct exb_request_state;

void exb_perf_handle_perf_act_event(struct exb_event ev);

//doesnt add itself to eloop
static int exb_event_act_init(struct exb_event *ev, int cmd, void *object, int arg) {
    ev->handle = exb_perf_handle_perf_act_event;
    ev->msg.u.iip.arg1 = arg;
    ev->msg.u.iip.arg2 = cmd;
    ev->msg.u.iip.argp = object;
    return EXB_OK;
}

#endif// EXB_PERF_ACT_H