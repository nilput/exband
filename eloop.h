#ifndef ELOOP_H
#define ELOOP_H
#include "cpb.h"
#include "string.h"
#include "utils.h"
#define ELOOP_SLEEP_TIME 50
struct cpb_event; //fwd
struct cpb_event_handler_itable {
    void (*handle)(struct cpb_event event);
    void (*destroy)(struct cpb_event event);
};
struct cpb_msg {
    int arg1;
    int arg2;
    void *argp;
};
struct cpb_event {
    struct cpb_event_handler_itable *itable;
    struct cpb_msg msg;
};
struct cpb_eloop {
    struct cpb_event* events;
    struct cpb *cpb; //not owned, must outlive
    unsigned ev_id; //counter
    int head;
    int tail; //tail == head means full, tail == head - 1 means full
    int cap;
};
static int cpb_eloop_len(struct cpb_eloop *eloop) {
    if (eloop->tail > eloop->head) {
        return eloop->tail - eloop->head;
    }
    return eloop->head - eloop->tail;
}
static int cpb_eloop_resize(struct cpb_eloop *eloop, int sz);
static int cpb_eloop_init(struct cpb_eloop *eloop, struct cpb* cpb_ref, int sz) {
    memset(eloop, 0, sizeof *eloop);
    eloop->cpb = cpb_ref;
    if (sz != 0) {
        return cpb_eloop_resize(eloop, sz);
    }
    return CPB_OK;
}
static int cpb_eloop_deinit(struct cpb_eloop *eloop) {
    //TODO destroy pending events
    cpb_free(eloop->cpb, eloop->events);
    return CPB_OK;
}

static int cpb_eloop_resize(struct cpb_eloop *eloop, int sz) {
    cpb_assert_h(!!eloop->cpb, "");
    void *p = cpb_malloc(eloop->cpb, sizeof(struct cpb_event) * sz);

    if (!p) {
        return CPB_NOMEM_ERR;
    }
    //this can be optimized
    struct cpb_event *events = p;
    int idx = 0;
    for (int i=eloop->head; ; i++) {
        if (i >= eloop->cap) 
            i = 0;
        if (i == eloop->tail)
            break;
        if (idx >= (sz - 1)) {
            break; //some events were lost because size is less than needed!
        }
        events[idx] = eloop->events[i];
        idx++;
    }
    cpb_free(eloop->cpb, eloop->events);
    int prev_len = cpb_eloop_len(eloop);
    eloop->events = p;
    eloop->head = 0;
    eloop->tail = idx;
    eloop->cap = sz;
    int new_len = cpb_eloop_len(eloop);
    cpb_assert_h(prev_len == new_len, "");
    
    return CPB_OK;
}
//copies event, [eventually calls ev->destroy()]
static int cpb_eloop_append(struct cpb_eloop *eloop, struct cpb_event ev) {
    eloop->ev_id++;
    if (cpb_eloop_len(eloop) == eloop->cap - 1) {
        int nsz = eloop->cap * 2;
        int rv = cpb_eloop_resize(eloop, nsz > 0 ? nsz : 4);
        if (rv != CPB_OK)
            return rv;
    }
    eloop->events[eloop->tail] = ev;
    eloop->tail++;
    if (eloop->tail >= eloop->cap) //eloop->tail %= eloop->cap;
        eloop->tail = 0;
    return CPB_OK;
}
static int cpb_eloop_pop_next(struct cpb_eloop *eloop, struct cpb_event *ev_out) {
    if (cpb_eloop_len(eloop) == 0)
        return CPB_OUT_OF_RANGE_ERR;
    struct cpb_event ev = eloop->events[eloop->head];
    eloop->head++;
    if (eloop->head >= eloop->cap) //eloop->head %= eloop->cap;
        eloop->head = 0;
    *ev_out = ev;
    return CPB_OK;
}
static struct cpb_error cpb_eloop_run(struct cpb_eloop *eloop) {
    while (1) {
        struct cpb_event ev;
        int rv = cpb_eloop_pop_next(eloop, &ev);
        if (rv != CPB_OK) {
            if (rv != CPB_OUT_OF_RANGE_ERR) {
                //serious error
                struct cpb_error err = cpb_make_error(rv);
                return err;
            }
            cpb_sleep(ELOOP_SLEEP_TIME);
            continue;
        }
        //handle event
        ev.itable->handle(ev);
        ev.itable->destroy(ev);
    }
    
    return cpb_make_error(CPB_OK);
}

#endif// ELOOP_H