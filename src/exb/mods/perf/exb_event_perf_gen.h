/*
These two files simulate the behavior of the http server without sockets involved
this is to test the performance of the eloop and inter-thread communication
exb_event_perf_gen: generates events to be handled by exb_evenet_perf_act, this analogous to what happens in http_server.c
exb_event_perf_act: handles events, this analogous to what happens in http_server_events.c
*/

#ifndef EXB_EVENT_PERF_TEST_H
#define EXB_EVENT_PERF_TEST_H
#include "../../exb.h"
#include "../../exb_event.h"
#include "../../exb_eloop.h"
#include "exb_event_perf_act.h"
struct exb_perf_gen {
    struct exb_eloop *eloop; //not owned, must outlive
    size_t total; //predefined
    size_t syns; //increased by us
    size_t acks; //increased by threads

    size_t prog; //progress report
};

//analogous to EXB_HTTP_MIN_DELAY
#define exb_perf_gen_MIN_DELAY 5

#define PER_LOOP 5000
//analogous to server_listen_once
static void exb_perf_gen_once(struct exb_perf_gen *t) {
    //exb_sleep(1);
    struct exb_event ev;
    for (int i=0; i<PER_LOOP; i++) {
        exb_event_act_init(&ev, EXB_PERF_ACT_INIT, t, 0);
        exb_eloop_append(t->eloop, ev);
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
static void exb_perf_gen_loop(struct exb_event ev) {
    struct exb_perf_gen *t = ev.msg.u.iip.argp;
    exb_perf_gen_once(t);
    struct exb_event new_ev = {
                               .handle = exb_perf_gen_loop,
                               .msg = {
                                .u.iip.argp = t
                                }
                              };
    struct exb_eloop *eloop = t->eloop;
    exb_assert_h(!!eloop, "");
    exb_eloop_append_delayed(eloop, new_ev, exb_perf_gen_MIN_DELAY, 1);
}
static void exb_perf_gen_init(struct exb_perf_gen *t, struct exb_eloop *eloop, size_t total) {
    t->total = total;
    t->syns = 0;
    t->acks = 0;
    t->prog = 0;
    t->eloop = eloop;
}
static struct exb_error exb_perf_gen_begin(struct exb_perf_gen *t) {
    struct exb_event new_ev = {
                               .handle = exb_perf_gen_loop,
                               .msg.u.iip.argp = t
                              };
    exb_perf_gen_loop(new_ev);
    return exb_make_error(EXB_OK);
}
#endif // EXB_EVENT_PERF_TEST_H