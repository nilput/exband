#ifndef EXB_TS_EVENT_QUEUE_H
#define EXB_TS_EVENT_QUEUE_H
#include <string.h>
#include <pthread.h>
#include "exb_errors.h"
#include "exb_event.h"
#include "exb.h"

struct exb;
struct exb_ts_event_queue {
    struct exb *exb; //not owned, must outlive
    pthread_mutex_t mtx;

    struct exb_event *events;
    int head;
    int tail; //tail == head means empty, tail == head - 1 means full
    int cap;
};
static int exb_ts_event_queue_len_u(struct exb_ts_event_queue *tq) {
    if (tq->tail >= tq->head) {
        return tq->tail - tq->head;
    }
    return tq->cap - tq->head + tq->tail;
}

static int exb_ts_event_queue_len(struct exb_ts_event_queue *tq) {
    if (pthread_mutex_lock(&tq->mtx) != 0) {
        return 0;
    }
    int len = exb_ts_event_queue_len_u(tq);
ret:
    pthread_mutex_unlock(&tq->mtx);
    return len;
}
static int exb_ts_event_queue_resize(struct exb_ts_event_queue *tq, int sz);
static int exb_ts_event_queue_resize_u(struct exb_ts_event_queue *tq, int sz);
static int exb_ts_event_queue_append(struct exb_ts_event_queue *tq, struct exb_event event) {
    if (pthread_mutex_lock(&tq->mtx) != 0) {
        return EXB_MUTEX_LOCK_ERROR;
    }
    int err = EXB_OK;
    if (exb_ts_event_queue_len_u(tq) == tq->cap - 1) {
        int nsz = tq->cap * 2;
        err = exb_ts_event_queue_resize_u(tq, nsz > 0 ? nsz : 4);
        if (err != EXB_OK)
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
static int exb_ts_event_queue_resize_u(struct exb_ts_event_queue *tq, int sz) {
    int err = EXB_OK;
    exb_assert_h(!!tq->exb, "");
    void *p = exb_malloc(tq->exb, sizeof(struct exb_event) * sz);

    if (!p) {
        return EXB_NOMEM_ERR;
    }
    //this can be optimized, see also eloop
    struct exb_event *events = p;
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
    exb_free(tq->exb, tq->events);
    int prev_len = exb_ts_event_queue_len_u(tq);
    tq->events = p;
    tq->head = 0;
    tq->tail = idx;
    tq->cap = sz;
    int new_len = exb_ts_event_queue_len_u(tq);
    exb_assert_h(prev_len == new_len, "");

    return EXB_OK;
}
static int exb_ts_event_queue_resize(struct exb_ts_event_queue *tq, int sz) {
    if (pthread_mutex_lock(&tq->mtx) != 0) {
        return EXB_MUTEX_LOCK_ERROR;
    }
    int err = exb_ts_event_queue_resize_u(tq, sz);
ret:
    pthread_mutex_unlock(&tq->mtx);
    return err;
}


static int exb_ts_event_queue_pop_next(struct exb_ts_event_queue *tq, struct exb_event *event_out) {
    if (pthread_mutex_lock(&tq->mtx) != 0) {
        return EXB_MUTEX_LOCK_ERROR;
    }
    int err = EXB_OK;
    if (exb_ts_event_queue_len_u(tq) == 0) {
        err = EXB_OUT_OF_RANGE_ERR;
        goto ret;
    }
    {
        struct exb_event event = tq->events[tq->head];
        tq->head++;
        if (tq->head >= tq->cap) //tq->head %= tq->cap;
            tq->head = 0;
        *event_out = event;
    }
ret:
    pthread_mutex_unlock(&tq->mtx);
    return err;
}

static int exb_ts_event_queue_pop_many(struct exb_ts_event_queue *tq, struct exb_event *events_out, int *nevents_out, int max_events) {
    if (pthread_mutex_lock(&tq->mtx) != 0) {
        return EXB_MUTEX_LOCK_ERROR;
    }
    int err = EXB_OK;

    int nevents = exb_ts_event_queue_len_u(tq);
    if (nevents > max_events)
        nevents = max_events;
    for (int i=0; i<nevents; i++){
        exb_assert_h(tq->head != tq->tail, "");
        events_out[i] = tq->events[tq->head];
        tq->head++;
        if (tq->head >= tq->cap) //tq->head %= tq->cap;
            tq->head = 0;
    }
    *nevents_out = nevents;
    #if defined(EXB_ASSERTS) && 0 
        //TODO: MOVE TO A TEST SUITE
        int i = tq->head;
        for (int idx = nevents; idx > 0;) {
            idx--;
            i = i == 0 ? tq->cap - 1 : i - 1;
            exb_assert_h(memcmp(events_out + idx, tq->events + i, sizeof(struct exb_event)) == 0, "");
        }
    #endif

bnret:
    pthread_mutex_unlock(&tq->mtx);
    return err;
}


static int exb_ts_event_queue_init(struct exb_ts_event_queue *tq, struct exb* exb_ref, int sz) {
    memset(tq, 0, sizeof *tq);
    tq->exb = exb_ref;
    pthread_mutex_init(&tq->mtx, NULL);
    if (sz != 0) {
        return exb_ts_event_queue_resize_u(tq, sz);
    }
    return EXB_OK;
}
static int exb_ts_event_queue_deinit(struct exb_ts_event_queue *tq) {
    //TODO destroy pending events
    exb_free(tq->exb, tq->events);
    pthread_mutex_destroy(&tq->mtx);
    return EXB_OK;
}

#endif // EXB_TS_EVENT_QUEUE_H