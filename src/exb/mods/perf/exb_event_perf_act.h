#ifndef CPB_PERF_ACT_H
#define CPB_PERF_ACT_H
#include "../../cpb_eloop.h"

struct cpb_perf_act_state {
    struct cpb_eloop *eloop; //not owned, must outlive
};

enum cpb_event_perf_act_cmd {
    CPB_PERF_ACT_INIT, //just initialized with no data, .argp : rqstate
    CPB_PERF_ACT_CONTINUE, //forked in a persistent connection, .argp : rqstate
    CPB_PERF_ACT_READ, /*ask it to read, .argp : rqstate*/
    CPB_PERF_ACT_SEND, /*ask it to write, .argp : rqstate*/
    CPB_PERF_ACT_DID_READ,  /*inform about async read result, .argp : rqstate*/
    CPB_PERF_ACT_DID_WRITE, /*inform about async write result, .argp : rqstate*/
    CPB_PERF_ACT_WRITE_IO_ERROR, /*inform about error during async read/write, .argp : rqstate*/
    CPB_PERF_ACT_READ_IO_ERROR,
    CPB_PERF_ACT_INPUT_BUFFER_FULL, /*inform about a full buffer during async read, .argp : rqstate*/
    CPB_PERF_ACT_CLIENT_CLOSED, //.argp : rqstate
    CPB_PERF_ACT_CANCEL, /*.argp: http server, .arg1: socket*/
};

struct cpb_request_state;

void cpb_perf_handle_perf_act_event(struct cpb_event ev);

//doesnt add itself to eloop
static int cpb_event_act_init(struct cpb_event *ev, int cmd, void *object, int arg) {
    ev->handle = cpb_perf_handle_perf_act_event;
    ev->msg.u.iip.arg1 = arg;
    ev->msg.u.iip.arg2 = cmd;
    ev->msg.u.iip.argp = object;
    return CPB_OK;
}

#endif// CPB_PERF_ACT_H