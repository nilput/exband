/*
    Asynchornous Event loop.
*/
#ifndef EVLOOP_H
#define EVLOOP_H

#include <stdbool.h>
#include <string.h>
#include "exb.h"
#include "exb_build_config.h"
#include "exb_utils.h"
#include "exb_task.h"
#include "exb_ts_event_queue.h"
#include "exb_event.h"
#include "exb_time.h"
#include "exb_buffer_recycle_list.h"

struct exb_event; //fwd

struct exb_delayed_event {
    struct exb_timestamp due; //unix time + delay
    struct exb_event event;
    unsigned char tolerate_preexec;
};

struct exb_delayed_event_node {
    struct exb_delayed_event cur;
    unsigned char storage; //>=EXB_EVLOOP_DEVENT_BUFFER_COUNT means malloc'd
    struct exb_delayed_event_node *next;
};

struct exb_evloop;
static int exb_evloop_resize(struct exb_evloop *evloop, int sz);
static int exb_evloop_append_delayed(struct exb_evloop *evloop, struct exb_event ev, int ms, int tolerate_preexec);

struct exb_evloop {
    struct exb *exb; //not owned, must outlive
    //Thread safe event queue, this is where other threads put events in.
    struct exb_ts_event_queue tsq; 
    
    int evloop_id; //A number assigned for each event loop [0, 1, 2, ...]
    unsigned ev_id; //counter
    int do_stop;

    struct exb_delayed_event_node* devents;
    int devents_len;
    int ntasks;

    struct exb_timestamp current_timestamp;
    struct exb_event* events;
    int head;
    int tail; //tail == head means full, tail == head - 1 means full
    int cap;

    struct exb_buffer_recycle_list buff_cyc;

    struct exb_delayed_event_node devents_buffer[EXB_EVLOOP_DEVENT_BUFFER_COUNT];
};

//_devent_ functions are for delayed events which are kept in a linked list
//_queue_ functions are for regular events which are kept in an array as a queue

static int exb_evloop_devent_push(struct exb_evloop *evloop, struct exb_event event, struct exb_timestamp due, int tolerate_prexec) {
    void *p = NULL;
    int storage = EXB_EVLOOP_DEVENT_BUFFER_COUNT;
    for (int i=0; i<EXB_EVLOOP_DEVENT_BUFFER_COUNT; i++) {
        if (evloop->devents_buffer[i].storage != i) {
            p = &evloop->devents_buffer[i];
            storage = i;
            break;
        }
    }
    if (!p)
        p = exb_malloc(evloop->exb, sizeof(struct exb_delayed_event_node));
    if (!p)
        return EXB_NOMEM_ERR;
    struct exb_delayed_event_node *node = p;
    node->storage = storage;
    node->cur.event = event;
    node->cur.due = due;
    node->cur.tolerate_preexec = tolerate_prexec;
    if (evloop->devents == NULL || (exb_timestamp_cmp(evloop->devents->cur.due, node->cur.due) > 0)) {
        node->next = evloop->devents;
        evloop->devents = node;
    }
    else {
        struct exb_delayed_event_node *at = evloop->devents;
        while (at->next && (exb_timestamp_cmp(at->next->cur.due, node->cur.due) < 0))
            at = at->next;
        node->next = at->next;
        at->next = node;
    }
    evloop->devents_len++;
    return EXB_OK;
}

static struct exb_timestamp exb_evloop_next_delayed_event_timestamp(struct exb_evloop *evloop) {
    struct exb_delayed_event_node *at = evloop->devents;
    if (!at) {
        return evloop->current_timestamp;
    }
    return at->cur.due;
}

static int exb_evloop_queue_len(struct exb_evloop *evloop) {
    if (evloop->tail >= evloop->head) {
        return evloop->tail - evloop->head;
    }
    return evloop->cap - evloop->head + evloop->tail;
}

static int exb_evloop_queue_pop_next(struct exb_evloop *evloop, struct exb_event *ev_out) {
    if (exb_evloop_queue_len(evloop) == 0)
        return EXB_OUT_OF_RANGE_ERR;
    struct exb_event ev = evloop->events[evloop->head];
    evloop->head++;
    if (evloop->head >= evloop->cap) //evloop->head %= evloop->cap;
        evloop->head = 0;
    *ev_out = ev;
    return EXB_OK;
}

static int exb_evloop_devent_pop_next(struct exb_evloop *evloop, struct exb_event *ev_out) {
    if (evloop->devents_len < 1)
        return EXB_OUT_OF_RANGE_ERR;
    
    struct exb_delayed_event_node *next = evloop->devents->next;
    struct exb_delayed_event_node *head = evloop->devents;
    *ev_out = head->cur.event;
    if (head->storage < EXB_EVLOOP_DEVENT_BUFFER_COUNT) {
        //internally allocated
        exb_assert_h(evloop->devents_buffer[head->storage].storage == head->storage, "");
        evloop->devents_buffer[head->storage].storage = EXB_EVLOOP_DEVENT_BUFFER_COUNT;
    }
    else {
        exb_free(evloop->exb, head);
    }
    
    evloop->devents = next;
    evloop->devents_len--;
    return EXB_OK;
}

static int exb_evloop_devent_len(struct exb_evloop *evloop) {
    return evloop->devents_len;
}

static struct exb_delayed_event *exb_evloop_devent_peek_next(struct exb_evloop *evloop) {
    if (exb_evloop_devent_len(evloop) == 0)
        return NULL;
    return &evloop->devents->cur;
}

static int exb_evloop_pop_next(struct exb_evloop *evloop, struct exb_event *ev_out) {
    struct exb_delayed_event *dev = exb_evloop_devent_peek_next(evloop);
    if (dev != NULL && exb_timestamp_cmp(dev->due, evloop->current_timestamp) <= 0) {
        return exb_evloop_devent_pop_next(evloop, ev_out);
    }
    return exb_evloop_queue_pop_next(evloop, ev_out);
}

static int exb_evloop_len(struct exb_evloop *evloop) {
    return exb_evloop_queue_len(evloop) + exb_evloop_devent_len(evloop);
}

static int exb_evloop_init(struct exb_evloop *evloop, int evloop_id, struct exb* exb_ref, int sz) {
    memset(evloop, 0, sizeof *evloop);
    evloop->devents_len = 0;
    evloop->exb = exb_ref;
    evloop->evloop_id = evloop_id;
    evloop->do_stop = 0;

    evloop->ntasks = 0;
    evloop->current_timestamp = exb_timestamp(0);

    int rv;
    if ((rv = exb_buffer_recycle_list_init(exb_ref, &evloop->buff_cyc)) != EXB_OK) {
        return rv;
    }
    rv = exb_ts_event_queue_init(&evloop->tsq, exb_ref, 16);
    if (rv != EXB_OK) {
        exb_buffer_recycle_list_deinit(exb_ref, &evloop->buff_cyc);
        return rv;
    }
    if (sz != 0) {
        rv = exb_evloop_resize(evloop, sz);
        if (rv != EXB_OK) {
            exb_buffer_recycle_list_deinit(exb_ref, &evloop->buff_cyc);
            exb_ts_event_queue_deinit(&evloop->tsq);
            return rv;
        }
    }
    for (int i=0; i<EXB_EVLOOP_DEVENT_BUFFER_COUNT; i++)
        evloop->devents_buffer[i].storage = EXB_EVLOOP_DEVENT_BUFFER_COUNT;
    return EXB_OK;
}

static int exb_evloop_deinit(struct exb_evloop *evloop) {
    //TODO destroy pending events
    fprintf(stderr, "freeing evloop %p\n", evloop);
    exb_buffer_recycle_list_deinit(evloop->exb, &evloop->buff_cyc);
    exb_ts_event_queue_deinit(&evloop->tsq);
    exb_free(evloop->exb, evloop->events);
    return EXB_OK;
}

static int exb_evloop_resize(struct exb_evloop *evloop, int sz) {
    exb_assert_h(!!evloop->exb, "");
    void *p = exb_malloc(evloop->exb, sizeof(struct exb_event) * sz);

    if (!p) {
        return EXB_NOMEM_ERR;
    }
    //this can be optimized, see also taskqueue
    struct exb_event *events = p;
    int idx = 0;
    for (int i=evloop->head; ; i++) {
        if (i >= evloop->cap) 
            i = 0;
        if (i == evloop->tail)
            break;
        if (idx >= (sz - 1)) {
            break; //some events were lost because size is less than needed!
        }
        events[idx] = evloop->events[i];
        idx++;
    }
    exb_free(evloop->exb, evloop->events);
    int prev_len = exb_evloop_len(evloop);
    evloop->events = p;
    evloop->head = 0;
    evloop->tail = idx;
    evloop->cap = sz;
    int new_len = exb_evloop_len(evloop);
    exb_assert_h(prev_len == new_len, "");
    
    return EXB_OK;
}

//copies event, [eventually calls ev->destroy()]
static int exb_evloop_append(struct exb_evloop *evloop, struct exb_event ev) {
    evloop->ev_id++;
    if (exb_evloop_queue_len(evloop) >= evloop->cap - 1) {
        int nsz = evloop->cap * 2;
        int rv = exb_evloop_resize(evloop, nsz > 0 ? nsz : 4);
        if (rv != EXB_OK)
            return rv;
    }
    evloop->events[evloop->tail] = ev;
    evloop->tail++;
    if (evloop->tail >= evloop->cap) //evloop->tail %= evloop->cap;
        evloop->tail = 0;
    return EXB_OK;
}

//see also thread_pool_append_many
static int exb_evloop_append_many(struct exb_evloop *evloop, struct exb_event *events, int nevents) {
    
    if ((exb_evloop_queue_len(evloop) + nevents) >= evloop->cap - 1) {
        int nsz = evloop->cap ? evloop->cap * 2 : 4;
        while ((exb_evloop_queue_len(evloop) + nevents) >= nsz - 1)
            nsz *= 2;
        int rv = exb_evloop_resize(evloop, nsz);
        if (rv != EXB_OK)
            return rv;
    }
    exb_assert_h((exb_evloop_queue_len(evloop) + nevents) < evloop->cap - 1, "");
    evloop->ev_id += nevents;

    if (evloop->head > evloop->tail) {
        //example: cap=8
        //         []
        //indices: 0  1  2  3  4  5  6  7
        //               ^tail       ^head
        //               ^^^^^^^^^^
        //               at tail
        // no at head
        exb_assert_h(evloop->tail + nevents < evloop->head, "");
        memcpy(evloop->events + evloop->tail, events, nevents * sizeof(struct exb_event));
        evloop->tail += nevents;
    }
    else {
        //example: cap=8
        //         []
        //indices: 0  1  2  3  4  5  6  7
        //         ^^^^  ^head ^tail
        //         ^           ^^^^^^^^^^
        //         ^           ^^at tail^
        //         ^
        //         at head
        int at_tail = evloop->cap - evloop->tail;
        if (at_tail > nevents) {
            at_tail = nevents;
        }
        memcpy(evloop->events + evloop->tail, events, at_tail * sizeof(struct exb_event));
        int remainder = nevents - at_tail;
        if (remainder > 0) {
            exb_assert_h(evloop->head > remainder, "");
            //A STUPID BUG WAS FIXED HERE!
            memcpy(evloop->events, events + at_tail, remainder * sizeof(struct exb_event));
            evloop->tail = remainder;
        }
        else {
            exb_assert_h(remainder == 0, "");
            evloop->tail += at_tail;
            if (evloop->tail >= evloop->cap)
                evloop->tail = 0;
        }
    }
    #if defined(EXB_ASSERTS) && 0 
        //TODO: MOVE TO A TEST SUITE
        int i = evloop->tail;
        for (int idx = nevents; idx > 0;) {
            exb_assert_h(i != evloop->head, "");
            idx--;
            i = i == 0 ? evloop->cap - 1 : i - 1;
            exb_assert_h(memcmp(events + idx, evloop->events + i, sizeof(struct exb_event)) == 0, "");
        }
    #endif
    return EXB_OK;
}

/*Threadsafe append event to be sent to the event loop from another thread*/
static int exb_evloop_threadsend_append(struct exb_evloop *evloop, struct exb_event event) {
    int rv = exb_ts_event_queue_append(&evloop->tsq, event);
    return rv;
}

/*Threadsafe pop event received from another thread to the event loop*/
static int exb_evloop_threadrecv_pop_many(struct exb_evloop *evloop, struct exb_event *events_out, int *nevents, int max_events) {
    int rv = exb_ts_event_queue_pop_many(&evloop->tsq, events_out, nevents, max_events);
    return rv;
}

//copies event, [eventually calls ev->destroy()]
static int exb_evloop_append_delayed(struct exb_evloop *evloop, struct exb_event ev, int ms, int tolerate_preexec) {
    return exb_evloop_devent_push(evloop, ev, exb_timestamp_add_usec(evloop->current_timestamp, ms * 1000), tolerate_preexec);
}

static int exb_evloop_fatal(struct exb_evloop *evloop, struct exb_error err) {
    abort();
}

//thread safe
static int exb_evloop_stop(struct exb_evloop *evloop) {
    struct exb_event ev;
    evloop->do_stop = 1;
    return EXB_OK;
}

// Returns true if there are any events from other threads which can be received.
static bool exb_evloop_any_receivables(struct exb_evloop *evloop) {
    return !exb_ts_event_queue_is_empty(&evloop->tsq);
}

//recieves events from other threads
static int exb_evloop_receive(struct exb_evloop *evloop) {
    struct exb_event events[EXB_EVLOOP_TMP_EVENTS_SZ];
    int nevents = 0;
    int rv = EXB_OK;
    //FIXME: make sure this loop is safe in case of errors
    for (int i=0;
         (i == 0 || nevents == EXB_EVLOOP_TMP_EVENTS_SZ);
          i++) 
    {
        rv = exb_evloop_threadrecv_pop_many(evloop, events, &nevents, EXB_EVLOOP_TMP_EVENTS_SZ);
        if (rv != EXB_OK) {
            if (rv == EXB_OUT_OF_RANGE_ERR) {
                return EXB_OK;
            }
            return rv;
        }
        rv = exb_evloop_append_many(evloop, events, nevents);
        if (rv != EXB_OK) {
            exb_assert_h(0, ""); //this 'll ruin order
            for (int i=0; i<nevents; i++) {
                struct exb_event *ev = events + i;
                rv = exb_evloop_threadsend_append(evloop, *ev);
                if (rv != EXB_OK) {
                    exb_evloop_fatal(evloop, exb_make_error(rv));
                }
            }
            return rv;
        }
    }
    
    return EXB_OK;
}

static int exb_evloop_run(struct exb_evloop *evloop) {
    #define EXB_EVLOOP_NPROCESS_OFFLOAD 64

    evloop->current_timestamp = exb_timestamp_now();
    while (!evloop->do_stop) {
        struct exb_event ev;
        
         //TODO: [scheduling] sometimes ignore timed events and pop from array anyways to ensure progress

        int rv = EXB_OUT_OF_RANGE_ERR;
        for (int i = 0; i < EXB_EVLOOP_QUEUE_BATCH_SIZE; i++) {
            rv = exb_evloop_queue_pop_next(evloop, &ev);
            if (rv != EXB_OK)
                break;
            exb_assert_s(!!ev.handle, "ev.handle == NULL");
            ev.handle(ev);
        }
        if (rv != EXB_OK && rv != EXB_OUT_OF_RANGE_ERR) {
            //serious error
            return rv;
        }
        
        int dlen = exb_evloop_devent_len(evloop);
        for (int i = 0; i < dlen; i++) {
            rv = exb_evloop_devent_pop_next(evloop, &ev);
            if (rv != EXB_OK)
                break;
            exb_assert_s(!!ev.handle, "ev.handle == NULL");
            ev.handle(ev);
        }
        
        if (rv != EXB_OK && rv != EXB_OUT_OF_RANGE_ERR) {
            //serious error
            return rv;
        }
        //Receive events from other threads.
        if (exb_evloop_any_receivables(evloop)) {
            exb_evloop_receive(evloop);
        }
        
        if (exb_evloop_queue_len(evloop) == 0) {
            exb_sleep_until_timestamp_ex(evloop->current_timestamp, exb_evloop_next_delayed_event_timestamp(evloop));
        }
        evloop->current_timestamp = exb_timestamp_now();
    }
    
    return EXB_OK;
}




//TODO: optimize to recycle or use custom allocator
static int exb_evloop_alloc_buffer(struct exb_evloop *evloop, size_t size, char **buff_out, size_t *cap_out) {
    void *p;
    size_t cap;
    if (exb_buffer_recycle_list_pop(evloop->exb, &evloop->buff_cyc, size, &p, &cap) == EXB_OK) {
        *buff_out = p;
        *cap_out = cap;
        return EXB_OK;
    }
    *buff_out = exb_malloc(evloop->exb, size);
    if (!*buff_out) {
        return EXB_NOMEM_ERR;
    }
    *cap_out = size;
    return EXB_OK;
}

static int exb_evloop_realloc_buffer(struct exb_evloop *evloop, char *buff, size_t new_size, char **buff_out, int *cap_out) {
    char *new_buff = exb_realloc(evloop->exb, buff, new_size);
    if (!new_buff) {
        return EXB_NOMEM_ERR;
    }
    *buff_out = new_buff;
    *cap_out = new_size;
    return EXB_OK;
}

static void exb_evloop_release_buffer(struct exb_evloop *evloop, char *buff, size_t capacity) {
    if (exb_buffer_recycle_list_push(evloop->exb, &evloop->buff_cyc, buff, capacity) == EXB_OK)
        return;
    exb_free(evloop->exb, buff);
}

#endif// EVLOOP_H
