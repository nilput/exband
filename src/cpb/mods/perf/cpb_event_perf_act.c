#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include "../../cpb_eloop.h"
#include "../../cpb_threadpool.h"
#include "../../cpb_errors.h"
#include "../../http/http_server.h"
#include "../../http/http_server_internal.h"
#include "../../http/http_parse.h"
#include "cpb_event_perf_act.h"
#include "cpb_event_perf_gen.h"

#ifndef CPB_ASYNCHRONOUS_IO
#define CPB_ASYNCHRONOUS_IO 1
#endif

static void handle_perf_act_event(struct cpb_event ev);
static void destroy_perf_act_event(struct cpb_event ev);

struct cpb_event_handler_itable cpb_event_handler_perf_act_itable = {
    .handle = handle_perf_act_event,
    .destroy = destroy_perf_act_event,
};


static void cpb_perf_act_handle_fatal_error(struct cpb_perf_act_state *rqstate) {

}
/*This is not the only source of bytes the request has, see also cpb_perf_act_fork*/
struct cpb_error cpb_perf_act_read_from_client(struct cpb_perf_act_state *rqstate) {
    return cpb_make_error(CPB_OK);
}



void cpb_perf_act_async_read_from_client_runner(struct cpb_thread *thread, struct cpb_task *task) {
    struct cpb_perf_act_state *rqstate = task->msg.u.iip.argp;
    struct cpb_event ev;
    cpb_event_act_init(&ev, CPB_PERF_ACT_DID_READ, task->msg.u.iip.argp, 0);
    int perr = cpb_eloop_ts_append(rqstate->eloop, ev);
    if (perr != CPB_OK) {
        /*we cannot afford to have this fail*/
        cpb_perf_act_handle_fatal_error(rqstate);
    }
    
    return;
}
struct cpb_error cpb_perf_act_async_read_from_client(struct cpb_perf_act_state *rqstate) {
    
    struct cpb_threadpool *tp = rqstate->eloop->threadpool;
    struct cpb_task task;
    task.err = cpb_make_error(CPB_OK);
    task.run = cpb_perf_act_async_read_from_client_runner;
    task.msg.u.iip.arg1 = 0;
    task.msg.u.iip.arg2 = 0;
    task.msg.u.iip.argp = rqstate;
    int rv = cpb_eloop_push_task(rqstate->eloop, task, 10);
    

    return cpb_make_error(rv);
}
void cpb_perf_act_async_write_runner(struct cpb_thread *thread, struct cpb_task *task) {
    
    struct cpb_perf_act_state *rqstate = task->msg.u.iip.argp;
    
    struct cpb_event ev;

    cpb_event_act_init(&ev, CPB_PERF_ACT_DID_WRITE, rqstate, 0);
    
    int err = cpb_eloop_ts_append(rqstate->eloop, ev);
    if (err != CPB_OK) {
        /*we cannot afford to have this fail*/
        cpb_perf_act_handle_fatal_error(rqstate);
    }
    return;
}
struct cpb_error cpb_perf_act_async_write(struct cpb_perf_act_state *rqstate) {

    int rv = CPB_OK;

    struct cpb_threadpool *tp = rqstate->eloop->threadpool;
    struct cpb_task task;
    task.err = cpb_make_error(CPB_OK);
    task.run = cpb_perf_act_async_write_runner;
    task.msg.u.iip.arg1 = 0;
    task.msg.u.iip.arg2 = 0;
    task.msg.u.iip.argp = rqstate;
    rv = cpb_threadpool_push_task(tp, task);
    ret:
    
    return cpb_make_error(rv);

}


int cpb_perf_act_write(struct cpb_perf_act_state *rqstate) {
    return CPB_OK;
}

void cpb_perf_act_lifetime(struct cpb_perf_act_state *rqstate) {
    
}

static void handle_http_event(struct cpb_event ev);
void cpb_perf_act_on_request_done(struct cpb_perf_act_state *rqstate) {
    cpb_perf_act_lifetime(rqstate);
}
void cpb_perf_act_on_response_done(struct cpb_perf_act_state *rqstate) {
    cpb_perf_act_lifetime(rqstate);
}

int cpb_perf_act_end(struct cpb_response_state *rsp) {
    
    struct cpb_event ev;
    cpb_event_act_init(&ev, CPB_PERF_ACT_SEND, rsp->req_state, 0);
    
    return CPB_OK;
}


static void handle_perf_act_event(struct cpb_event ev) {
    struct cpb_error err = {0};
    enum cpb_event_perf_act_cmd cmd  = ev.msg.u.iip.arg2;
    struct cpb_perf_gen *gen = ev.msg.u.iip.argp;
    
    switch (cmd) {
        case CPB_PERF_ACT_INIT:
        case CPB_PERF_ACT_READ:
        case CPB_PERF_ACT_DID_READ:
        case CPB_PERF_ACT_INPUT_BUFFER_FULL:
        case CPB_PERF_ACT_CLIENT_CLOSED:
        case CPB_PERF_ACT_READ_IO_ERROR:
        case CPB_PERF_ACT_DID_WRITE:
        case CPB_PERF_ACT_SEND:
        case CPB_PERF_ACT_WRITE_IO_ERROR:
        case CPB_PERF_ACT_CONTINUE:
        {
        struct cpb_perf_act_state *rqstate = ev.msg.u.iip.argp;
        if (cmd == CPB_PERF_ACT_INIT || cmd == CPB_PERF_ACT_CONTINUE || cmd == CPB_PERF_ACT_READ) {
            
            if (CPB_ASYNCHRONOUS_IO) {
                cpb_perf_act_async_read_from_client(rqstate);
            }
            else {
                err = cpb_perf_act_read_from_client(rqstate);
            }
        }
        else if (cmd == CPB_PERF_ACT_SEND) {
            
            if (CPB_ASYNCHRONOUS_IO) {
                cpb_perf_act_async_write(rqstate);
            }
            else {
                int rv = cpb_perf_act_write(rqstate);
                if (rv != CPB_OK) {
                    err = cpb_make_error(rv);
                    goto ret;
                }
            }
        }
        else if (cmd == CPB_PERF_ACT_DID_READ) {
            int len  = ev.msg.u.iip.arg1;
            gen->acks++;
        }
        else if (cmd == CPB_PERF_ACT_DID_WRITE) {
            int len  = ev.msg.u.iip.arg1;
            if ((random() % 128) > 120) {
                cpb_perf_act_on_response_done(rqstate);
            }
        }
        else if (cmd == CPB_PERF_ACT_WRITE_IO_ERROR) {

        }
        else if (cmd == CPB_PERF_ACT_READ_IO_ERROR) {
        }
        else if (cmd == CPB_PERF_ACT_INPUT_BUFFER_FULL) {
        }
        else if (cmd == CPB_PERF_ACT_CLIENT_CLOSED) {

        }
        else{
            cpb_assert_h(0, "invalid cmd");
        }
    
    } 
    break;
    case CPB_PERF_ACT_CANCEL:
    {

    }
    break;
    default:
    {
        cpb_assert_h(0, "invalid cmd");
    }
    }
    
ret:
    /*TODO: print error or do something with it*/
    return;
}
static void destroy_perf_act_event(struct cpb_event ev) {
}

