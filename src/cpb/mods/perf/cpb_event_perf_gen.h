/*
These two files simulate the behavior of the http server without sockets involved
this is to test the performance of the eloop and inter-thread communication
cpb_event_perf_gen: generates events to be handled by cpb_evenet_perf_act, this analogous to what happens in http_server.c
cpb_event_perf_act: handles events, this analogous to what happens in http_server_events.c
*/

#ifndef CPB_EVENT_PERF_TEST_H
#define CPB_EVENT_PERF_TEST_H
#include "../../cpb.h"
#include "../../cpb_event.h"
#include "../../cpb_eloop.h"
#include "cpb_event_perf_act.h"
struct cpb_perf_gen {
    struct cpb_eloop *eloop; //not owned, must outlive
    size_t total; //predefined
    size_t syns; //increased by us
    size_t acks; //increased by threads

    size_t prog; //progress report
};

//analogous to CPB_HTTP_MIN_DELAY
#define cpb_perf_gen_MIN_DELAY 5
static struct cpb_event_handler_itable cpb_perf_gen_event_handler;
#define PER_LOOP 5000
//analogous to server_listen_once
static void cpb_perf_gen_once(struct cpb_perf_gen *t) {
    //cpb_sleep(1);
    struct cpb_event ev;
    for (int i=0; i<PER_LOOP; i++) {
        cpb_event_act_init(&ev, CPB_PERF_ACT_INIT, t, 0);
        cpb_eloop_append(t->eloop, ev);
        t->syns++;
    }
    
    if (t->acks >= t->total) {
        fprintf(stderr, "handled %llu events\n", t->acks);
        exit(0);
    }
    if (t->acks - t->prog > (t->total / 20)) {
        fprintf(stderr, "handled %llu events\n", t->acks);
        t->prog = t->acks;
    }
}
// server_listen_loop
static void cpb_perf_gen_loop(struct cpb_event ev) {
    struct cpb_perf_gen *t = ev.msg.u.iip.argp;
    cpb_perf_gen_once(t);
    struct cpb_event new_ev = {
                               .itable = &cpb_perf_gen_event_handler,
                               .msg = {
                                .u.iip.argp = t
                                }
                              };
    struct cpb_eloop *eloop = t->eloop;
    cpb_assert_h(!!eloop, "");
    cpb_eloop_append_delayed(eloop, new_ev, cpb_perf_gen_MIN_DELAY, 1);
}
static void cpb_perf_gen_init(struct cpb_perf_gen *t, struct cpb_eloop *eloop, size_t total) {
    t->total = total;
    t->syns = 0;
    t->acks = 0;
    t->eloop = eloop;
}
static struct cpb_error cpb_perf_gen_begin(struct cpb_perf_gen *t) {
    struct cpb_event new_ev = {
                               .itable = &cpb_perf_gen_event_handler,
                               .msg.u.iip.argp = t
                              };
    cpb_perf_gen_loop(new_ev);
    return cpb_make_error(CPB_OK);
}

static void cpb_perf_gen_event_destroy(struct cpb_event ev) {
    struct cpb_perf_gen *t = ev.msg.u.iip.argp;
}

static struct cpb_event_handler_itable cpb_perf_gen_event_handler = {
    .handle = cpb_perf_gen_loop,
    .destroy = cpb_perf_gen_event_destroy,
};


#endif // CPB_EVENT_PERF_TEST_H