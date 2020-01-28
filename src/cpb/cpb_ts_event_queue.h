#ifndef CPB_TS_EVENT_QUEUE_H
#define CPB_TS_EVENT_QUEUE_H
#include <string.h>
#include <pthread.h>
#include "cpb_errors.h"
#include "cpb_event.h"
#include "cpb.h"

struct cpb;
struct cpb_ts_event_queue {
    struct cpb *cpb; //not owned, must outlive
    pthread_mutex_t mtx;

    struct cpb_event *events;
    int head;
    int tail; //tail == head means full, tail == head - 1 means full
    int cap;
};


static int cpb_ts_event_queue_len_u(struct cpb_ts_event_queue *tq) {
    if (tq->tail >= tq->head) {
        return tq->tail - tq->head;
    }
    return tq->cap - tq->head + tq->tail;
}

static int cpb_ts_event_queue_len(struct cpb_ts_event_queue *tq) {
    if (pthread_mutex_lock(&tq->mtx) != 0) {
        return 0;
    }
    int len = cpb_ts_event_queue_len_u(tq);
ret:
    pthread_mutex_unlock(&tq->mtx);
    return len;
}
static int cpb_ts_event_queue_resize(struct cpb_ts_event_queue *tq, int sz);
static int cpb_ts_event_queue_resize_u(struct cpb_ts_event_queue *tq, int sz);
static int cpb_ts_event_queue_append(struct cpb_ts_event_queue *tq, struct cpb_event event) {
    if (pthread_mutex_lock(&tq->mtx) != 0) {
        return CPB_MUTEX_LOCK_ERROR;
    }
    int err = CPB_OK;
    if (cpb_ts_event_queue_len_u(tq) == tq->cap - 1) {
        int nsz = tq->cap * 2;
        err = cpb_ts_event_queue_resize_u(tq, nsz > 0 ? nsz : 4);
        if (err != CPB_OK)
            goto ret;
    }
    tq->events[tq->tail] = event;
    tq->tail++;
    if (tq->tail >= tq->cap) //tq->tail %= tq->cap;
        tq->tail = 0;
ret:
    pthread_mutex_unlock(&tq->mtx);
    return err;
}
static int cpb_ts_event_queue_resize_u(struct cpb_ts_event_queue *tq, int sz) {
    int err = CPB_OK;
    cpb_assert_h(!!tq->cpb, "");
    void *p = cpb_malloc(tq->cpb, sizeof(struct cpb_event) * sz);

    if (!p) {
        return CPB_NOMEM_ERR;
    }
    //this can be optimized, see also eloop
    struct cpb_event *events = p;
    int idx = 0;
    for (int i=tq->head; ; i++) {
        if (i >= tq->cap) 
            i = 0;
        if (i == tq->tail)
            break;
        if (idx >= (sz - 1)) {
            break; //some events were lost because size is less than needed!
        }
        events[idx] = tq->events[i];
        idx++;
    }
    cpb_free(tq->cpb, tq->events);
    int prev_len = cpb_ts_event_queue_len_u(tq);
    tq->events = p;
    tq->head = 0;
    tq->tail = idx;
    tq->cap = sz;
    int new_len = cpb_ts_event_queue_len_u(tq);
    cpb_assert_h(prev_len == new_len, "");

    return CPB_OK;
}
static int cpb_ts_event_queue_resize(struct cpb_ts_event_queue *tq, int sz) {
    if (pthread_mutex_lock(&tq->mtx) != 0) {
        return CPB_MUTEX_LOCK_ERROR;
    }
    int err = cpb_ts_event_queue_resize_u(tq, sz);
ret:
    pthread_mutex_unlock(&tq->mtx);
    return err;
}


static int cpb_ts_event_queue_pop_next(struct cpb_ts_event_queue *tq, struct cpb_event *event_out) {
    if (pthread_mutex_lock(&tq->mtx) != 0) {
        return CPB_MUTEX_LOCK_ERROR;
    }
    int err = CPB_OK;
    if (cpb_ts_event_queue_len_u(tq) == 0) {
        err = CPB_OUT_OF_RANGE_ERR;
        goto ret;
    }
    {
        struct cpb_event event = tq->events[tq->head];
        tq->head++;
        if (tq->head >= tq->cap) //tq->head %= tq->cap;
            tq->head = 0;
        *event_out = event;
    }
ret:
    pthread_mutex_unlock(&tq->mtx);
    return err;
}

static int cpb_ts_event_queue_pop_many(struct cpb_ts_event_queue *tq, struct cpb_event *events_out, int *nevents_out, int max_events) {
    if (pthread_mutex_lock(&tq->mtx) != 0) {
        return CPB_MUTEX_LOCK_ERROR;
    }
    int err = CPB_OK;

    int nevents = cpb_ts_event_queue_len_u(tq);
    if (nevents > max_events)
        nevents = max_events;
    for (int i=0; i<nevents; i++){
        cpb_assert_h(tq->head != tq->tail, "");
        events_out[i] = tq->events[tq->head];
        tq->head++;
        if (tq->head >= tq->cap) //tq->head %= tq->cap;
            tq->head = 0;
    }
    *nevents_out = nevents;
    #ifdef CPB_ASSERTS
        int i = tq->head;
        for (int idx = nevents; idx > 0;) {
            idx--;
            i = i == 0 ? tq->cap - 1 : i - 1;
            cpb_assert_h(memcmp(events_out + idx, tq->events + i, sizeof(struct cpb_event)) == 0, "");
        }
    #endif

bnret:
    pthread_mutex_unlock(&tq->mtx);
    return err;
}


static int cpb_ts_event_queue_init(struct cpb_ts_event_queue *tq, struct cpb* cpb_ref, int sz) {
    memset(tq, 0, sizeof *tq);
    tq->cpb = cpb_ref;
    pthread_mutex_init(&tq->mtx, NULL);
    if (sz != 0) {
        return cpb_ts_event_queue_resize_u(tq, sz);
    }
    return CPB_OK;
}
static int cpb_ts_event_queue_deinit(struct cpb_ts_event_queue *tq) {
    //TODO destroy pending events
    cpb_free(tq->cpb, tq->events);
    pthread_mutex_destroy(&tq->mtx);
    return CPB_OK;
}

#endif // CPB_TS_EVENT_QUEUE_H