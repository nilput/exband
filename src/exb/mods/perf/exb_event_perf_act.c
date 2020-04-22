#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include "../../exb_eloop.h"
#include "../../exb_threadpool.h"
#include "../../exb_errors.h"
#include "../../http/http_server.h"
#include "../../http/http_server_internal.h"
#include "../../http/http_parse.h"
#include "exb_event_perf_act.h"
#include "exb_event_perf_gen.h"
static void handle_perf_act_event(struct exb_event ev);

static void exb_perf_act_handle_fatal_error(struct exb_perf_act_state *rqstate) {

}
/*This is not the only source of bytes the request has, see also exb_perf_act_fork*/
struct exb_error exb_perf_act_read_from_client(struct exb_perf_act_state *rqstate) {
    return exb_make_error(EXB_OK);
}
void exb_perf_act_async_read_from_client_runner(struct exb_thread *thread, struct exb_task *task) {
    struct exb_perf_act_state *rqstate = task->msg.u.iip.argp;
    struct exb_event ev;
    exb_event_act_init(&ev, EXB_PERF_ACT_DID_READ, task->msg.u.iip.argp, 0);
    int perr = exb_eloop_ts_append(rqstate->eloop, ev);
    if (perr != EXB_OK) {
        /*we cannot afford to have this fail*/
        exb_perf_act_handle_fatal_error(rqstate);
    }
    
    return;
}
struct exb_error exb_perf_act_async_read_from_client(struct exb_perf_act_state *rqstate) {
    
    struct exb_threadpool *tp = rqstate->eloop->threadpool;
    struct exb_task task;
    task.err = exb_make_error(EXB_OK);
    task.run = exb_perf_act_async_read_from_client_runner;
    task.msg.u.iip.arg1 = 0;
    task.msg.u.iip.arg2 = 0;
    task.msg.u.iip.argp = rqstate;
    int rv = exb_eloop_push_task(rqstate->eloop, task, 10);
    return exb_make_error(rv);
}
void exb_perf_act_async_write_runner(struct exb_thread *thread, struct exb_task *task) {
    
    struct exb_perf_act_state *rqstate = task->msg.u.iip.argp;
    
    struct exb_event ev;

    exb_event_act_init(&ev, EXB_PERF_ACT_DID_WRITE, rqstate, 0);
    
    int err = exb_eloop_ts_append(rqstate->eloop, ev);
    if (err != EXB_OK) {
        /*we cannot afford to have this fail*/
        exb_perf_act_handle_fatal_error(rqstate);
    }
    return;
}
struct exb_error exb_perf_act_async_write(struct exb_perf_act_state *rqstate) {

    int rv = EXB_OK;

    struct exb_threadpool *tp = rqstate->eloop->threadpool;
    struct exb_task task;
    task.err = exb_make_error(EXB_OK);
    task.run = exb_perf_act_async_write_runner;
    task.msg.u.iip.arg1 = 0;
    task.msg.u.iip.arg2 = 0;
    task.msg.u.iip.argp = rqstate;
    rv = exb_threadpool_push_task(tp, task);
    ret:
    
    return exb_make_error(rv);

}


int exb_perf_act_write(struct exb_perf_act_state *rqstate) {
    return EXB_OK;
}

void exb_perf_act_lifetime(struct exb_perf_act_state *rqstate) {
    
}

void exb_perf_act_on_request_done(struct exb_perf_act_state *rqstate) {
    exb_perf_act_lifetime(rqstate);
}
void exb_perf_act_on_response_done(struct exb_perf_act_state *rqstate) {
    exb_perf_act_lifetime(rqstate);
}

int exb_perf_act_end(struct exb_response_state *rsp) {
    
    struct exb_event ev;
    exb_event_act_init(&ev, EXB_PERF_ACT_SEND, rsp, 0);
    
    return EXB_OK;
}


static void handle_perf_act_event(struct exb_event ev) {
    struct exb_error err = {0};
    enum exb_event_perf_act_cmd cmd  = ev.msg.u.iip.arg2;
    struct exb_perf_gen *gen = ev.msg.u.iip.argp;
    
    switch (cmd) {
        case EXB_PERF_ACT_INIT:
        case EXB_PERF_ACT_READ:
        case EXB_PERF_ACT_DID_READ:
        case EXB_PERF_ACT_INPUT_BUFFER_FULL:
        case EXB_PERF_ACT_CLIENT_CLOSED:
        case EXB_PERF_ACT_READ_IO_ERROR:
        case EXB_PERF_ACT_DID_WRITE:
        case EXB_PERF_ACT_SEND:
        case EXB_PERF_ACT_WRITE_IO_ERROR:
        case EXB_PERF_ACT_CONTINUE:
        {
        struct exb_perf_act_state *rqstate = ev.msg.u.iip.argp;
        if (cmd == EXB_PERF_ACT_INIT || cmd == EXB_PERF_ACT_CONTINUE || cmd == EXB_PERF_ACT_READ) {
            
            exb_perf_act_async_read_from_client(rqstate);
        
        }
        else if (cmd == EXB_PERF_ACT_SEND) {
            
            exb_perf_act_async_write(rqstate);
            
        }
        else if (cmd == EXB_PERF_ACT_DID_READ) {
            int len  = ev.msg.u.iip.arg1;
            gen->acks++;
        }
        else if (cmd == EXB_PERF_ACT_DID_WRITE) {
            int len  = ev.msg.u.iip.arg1;
            if ((random() % 128) > 120) {
                exb_perf_act_on_response_done(rqstate);
            }
        }
        else if (cmd == EXB_PERF_ACT_WRITE_IO_ERROR) {

        }
        else if (cmd == EXB_PERF_ACT_READ_IO_ERROR) {
        }
        else if (cmd == EXB_PERF_ACT_INPUT_BUFFER_FULL) {
        }
        else if (cmd == EXB_PERF_ACT_CLIENT_CLOSED) {

        }
        else{
            exb_assert_h(0, "invalid cmd");
        }
    
    } 
    break;
    case EXB_PERF_ACT_CANCEL:
    {

    }
    break;
    default:
    {
        exb_assert_h(0, "invalid cmd");
    }
    }
    
ret:
    /*TODO: print error or do something with it*/
    return;
}

void exb_perf_handle_perf_act_event(struct exb_event ev) {
    handle_perf_act_event(ev);
}