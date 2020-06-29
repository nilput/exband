#ifndef ELOOP_H
#define ELOOP_H
/*
Event loop
*/
#include "exb.h"
#include "string.h"
#include "exb_utils.h"
#include "exb_task.h"
#include "exb_ts_event_queue.h"
#include "exb_event.h"
#include "exb_threadpool.h"
#include "exb_time.h"
#include "exb_buffer_recycle_list.h"
#define ELOOP_SLEEP_TIME_MS 1
#define EXB_ELOOP_TMP_EVENTS_SZ 512
#define EXB_ELOOP_TASK_BUFFER_COUNT 256
//a small storage for delayed events to reduce calls to malloc, must be <= 255
#define EXB_ELOOP_DEVENT_BUFFER_COUNT 32
struct exb_event; //fwd
struct exb_threadpool;

struct exb_delayed_event {
    struct exb_timestamp due; //unix time + delay
    struct exb_event event;
    unsigned char tolerate_preexec;
};

struct exb_delayed_event_node {
    struct exb_delayed_event cur;
    unsigned char storage; //>=EXB_ELOOP_DEVENT_BUFFER_COUNT means malloc'd
    struct exb_delayed_event_node *next;
};

enum exb_eloop_event_cmd {
    EXB_ELOOP_FLUSH,
};

struct exb_eloop;
static int exb_eloop_flush_tasks(struct exb_eloop *eloop);

static void exb_eloop_handle_eloop_event(struct exb_event ev) {
    struct exb_eloop *eloop = ev.msg.u.iip.argp;
    int cmd = ev.msg.u.iip.arg1;
    if (cmd == EXB_ELOOP_FLUSH) {
        exb_eloop_flush_tasks(eloop);
    }
    else {
        exb_assert_h(0, "unknown cmd");
    }
}

static struct exb_event exb_eloop_make_eloop_event(struct exb_eloop *eloop, enum exb_eloop_event_cmd cmd) {
    struct exb_event ev;
    ev.handle = exb_eloop_handle_eloop_event;
    ev.msg.u.iip.arg1 = cmd;
    ev.msg.u.iip.argp = eloop;
    ev.msg.u.iip.arg2 = 0;
    return ev;
}

struct exb_eloop {
    struct exb *exb; //not owned, must outlive
    struct exb_threadpool *threadpool; //not owned, must outlive
    struct exb_ts_event_queue tsq;
    
    int eloop_id;

    unsigned ev_id; //counter
    int do_stop;

    struct exb_delayed_event_node* devents;
    int devents_len;
    int ntasks;
    int tasks_flush_min_delay_ms;
    struct exb_timestamp current_timestamp;
    struct exb_event* events;
    int head;
    int tail; //tail == head means full, tail == head - 1 means full
    int cap;

    struct exb_buffer_recycle_list buff_cyc;

    //this is done to collect a number of tasks before sending them to the threadpool
    struct exb_task task_buffer[EXB_ELOOP_TASK_BUFFER_COUNT];

    struct exb_delayed_event_node devents_buffer[EXB_ELOOP_DEVENT_BUFFER_COUNT];
};

//_d_ functions are for delayed events which are kept in a linked list
//_q_ functions are for regular events which are kept in an array as a queue

static int exb_eloop_d_push(struct exb_eloop *eloop, struct exb_event event, struct exb_timestamp due, int tolerate_prexec) {
    void *p = NULL;
    int storage = EXB_ELOOP_DEVENT_BUFFER_COUNT;
    for (int i=0; i<EXB_ELOOP_DEVENT_BUFFER_COUNT; i++) {
        if (eloop->devents_buffer[i].storage != i) {
            p = &eloop->devents_buffer[i];
            storage = i;
            break;
        }
    }
    if (!p)
        p = exb_malloc(eloop->exb, sizeof(struct exb_delayed_event_node));
    if (!p)
        return EXB_NOMEM_ERR;
    struct exb_delayed_event_node *node = p;
    node->storage = storage;
    node->cur.event = event;
    node->cur.due = due;
    node->cur.tolerate_preexec = tolerate_prexec;
    if (eloop->devents == NULL || (exb_timestamp_cmp(eloop->devents->cur.due, node->cur.due) > 0)) {
        node->next = eloop->devents;
        eloop->devents = node;
    }
    else {
        struct exb_delayed_event_node *at = eloop->devents;
        while (at->next && (exb_timestamp_cmp(at->next->cur.due, node->cur.due) < 0))
            at = at->next;
        node->next = at->next;
        at->next = node;
    }
    eloop->devents_len++;
    return EXB_OK;
}

static struct exb_timestamp exb_eloop_next_delayed_event_timestamp(struct exb_eloop *eloop) {
    struct exb_delayed_event_node *at = eloop->devents;
    if (!at) {
        return eloop->current_timestamp;
    }
    return at->cur.due;
}

static int exb_eloop_q_len(struct exb_eloop *eloop) {
    if (eloop->tail >= eloop->head) {
        return eloop->tail - eloop->head;
    }
    return eloop->cap - eloop->head + eloop->tail;
}

static int exb_eloop_q_pop_next(struct exb_eloop *eloop, struct exb_event *ev_out) {
    if (exb_eloop_q_len(eloop) == 0)
        return EXB_OUT_OF_RANGE_ERR;
    struct exb_event ev = eloop->events[eloop->head];
    eloop->head++;
    if (eloop->head >= eloop->cap) //eloop->head %= eloop->cap;
        eloop->head = 0;
    *ev_out = ev;
    return EXB_OK;
}
static int exb_eloop_d_pop_next(struct exb_eloop *eloop, struct exb_event *ev_out) {
    if (eloop->devents_len < 1)
        return EXB_OUT_OF_RANGE_ERR;
    
    struct exb_delayed_event_node *next = eloop->devents->next;
    struct exb_delayed_event_node *head = eloop->devents;
    *ev_out = head->cur.event;
    if (head->storage < EXB_ELOOP_DEVENT_BUFFER_COUNT) {
        //internally allocated
        exb_assert_h(eloop->devents_buffer[head->storage].storage == head->storage, "");
        eloop->devents_buffer[head->storage].storage = EXB_ELOOP_DEVENT_BUFFER_COUNT;
    }
    else {
        exb_free(eloop->exb, head);
    }
    
    eloop->devents = next;
    eloop->devents_len--;
    return EXB_OK;
}

static int exb_eloop_d_len(struct exb_eloop *eloop) {
    return eloop->devents_len;
}
static struct exb_delayed_event *exb_eloop_d_peek_next(struct exb_eloop *eloop) {
    if (exb_eloop_d_len(eloop) == 0)
        return NULL;
    return &eloop->devents->cur;
}
static int exb_eloop_pop_next(struct exb_eloop *eloop, struct exb_event *ev_out) {
    struct exb_delayed_event *dev = exb_eloop_d_peek_next(eloop);
    if (dev != NULL && exb_timestamp_cmp(dev->due, eloop->current_timestamp) <= 0) {
        return exb_eloop_d_pop_next(eloop, ev_out);
    }
    return exb_eloop_q_pop_next(eloop, ev_out);
}

static int exb_eloop_len(struct exb_eloop *eloop) {
    return exb_eloop_q_len(eloop) + exb_eloop_d_len(eloop);
}

static int exb_eloop_resize(struct exb_eloop *eloop, int sz);
static int exb_eloop_init(struct exb_eloop *eloop, int eloop_id, struct exb* exb_ref, struct exb_threadpool *tp, int sz) {
    memset(eloop, 0, sizeof *eloop);
    eloop->threadpool = tp;
    eloop->devents_len = 0;
    eloop->exb = exb_ref;
    eloop->eloop_id = eloop_id;
    eloop->do_stop = 0;

    eloop->ntasks = 0;
    eloop->current_timestamp = exb_timestamp(0);
    eloop->tasks_flush_min_delay_ms = 0;

    int rv;
    if ((rv = exb_buffer_recycle_list_init(exb_ref, &eloop->buff_cyc)) != EXB_OK) {
        return rv;
    }
    rv = exb_ts_event_queue_init(&eloop->tsq, exb_ref, 16);
    if (rv != EXB_OK) {
        exb_buffer_recycle_list_deinit(exb_ref, &eloop->buff_cyc);
        return rv;
    }
    if (sz != 0) {
        rv = exb_eloop_resize(eloop, sz);
        if (rv != EXB_OK) {
            exb_buffer_recycle_list_deinit(exb_ref, &eloop->buff_cyc);
            exb_ts_event_queue_deinit(&eloop->tsq);
            return rv;
        }
    }
    for (int i=0; i<EXB_ELOOP_DEVENT_BUFFER_COUNT; i++)
        eloop->devents_buffer[i].storage = EXB_ELOOP_DEVENT_BUFFER_COUNT;
    return EXB_OK;
}
static int exb_eloop_deinit(struct exb_eloop *eloop) {
    //TODO destroy pending events
    fprintf(stderr, "freeing eloop %p\n", eloop);
    exb_buffer_recycle_list_deinit(eloop->exb, &eloop->buff_cyc);
    exb_ts_event_queue_deinit(&eloop->tsq);
    exb_free(eloop->exb, eloop->events);
    return EXB_OK;
}

static int exb_eloop_resize(struct exb_eloop *eloop, int sz) {
    exb_assert_h(!!eloop->exb, "");
    void *p = exb_malloc(eloop->exb, sizeof(struct exb_event) * sz);

    if (!p) {
        return EXB_NOMEM_ERR;
    }
    //this can be optimized, see also taskqueue
    struct exb_event *events = p;
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
    exb_free(eloop->exb, eloop->events);
    int prev_len = exb_eloop_len(eloop);
    eloop->events = p;
    eloop->head = 0;
    eloop->tail = idx;
    eloop->cap = sz;
    int new_len = exb_eloop_len(eloop);
    exb_assert_h(prev_len == new_len, "");
    
    return EXB_OK;
}
//copies event, [eventually calls ev->destroy()]
static int exb_eloop_append(struct exb_eloop *eloop, struct exb_event ev) {
    eloop->ev_id++;
    if (exb_eloop_q_len(eloop) >= eloop->cap - 1) {
        int nsz = eloop->cap * 2;
        int rv = exb_eloop_resize(eloop, nsz > 0 ? nsz : 4);
        if (rv != EXB_OK)
            return rv;
    }
    eloop->events[eloop->tail] = ev;
    eloop->tail++;
    if (eloop->tail >= eloop->cap) //eloop->tail %= eloop->cap;
        eloop->tail = 0;
    return EXB_OK;
}

//see also thread_pool_append_many
static int exb_eloop_append_many(struct exb_eloop *eloop, struct exb_event *events, int nevents) {
    
    if ((exb_eloop_q_len(eloop) + nevents) >= eloop->cap - 1) {
        int nsz = eloop->cap ? eloop->cap * 2 : 4;
        while ((exb_eloop_q_len(eloop) + nevents) >= nsz - 1)
            nsz *= 2;
        int rv = exb_eloop_resize(eloop, nsz);
        if (rv != EXB_OK)
            return rv;
    }
    exb_assert_h((exb_eloop_q_len(eloop) + nevents) < eloop->cap - 1, "");
    eloop->ev_id += nevents;

    if (eloop->head > eloop->tail) {
        //example: cap=8
        //         []
        //indices: 0  1  2  3  4  5  6  7
        //               ^tail       ^head
        //               ^^^^^^^^^^
        //               at tail
        // no at head
        exb_assert_h(eloop->tail + nevents < eloop->head, "");
        memcpy(eloop->events + eloop->tail, events, nevents * sizeof(struct exb_event));
        eloop->tail += nevents;
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
        int at_tail = eloop->cap - eloop->tail;
        if (at_tail > nevents) {
            at_tail = nevents;
        }
        memcpy(eloop->events + eloop->tail, events, at_tail * sizeof(struct exb_event));
        int remainder = nevents - at_tail;
        if (remainder > 0) {
            exb_assert_h(eloop->head > remainder, "");
            //A STUPID BUG WAS FIXED HERE!
            memcpy(eloop->events, events + at_tail, remainder * sizeof(struct exb_event));
            eloop->tail = remainder;
        }
        else {
            exb_assert_h(remainder == 0, "");
            eloop->tail += at_tail;
            if (eloop->tail >= eloop->cap)
                eloop->tail = 0;
        }
    }
    #if defined(EXB_ASSERTS) && 0 
        //TODO: MOVE TO A TEST SUITE
        int i = eloop->tail;
        for (int idx = nevents; idx > 0;) {
            exb_assert_h(i != eloop->head, "");
            idx--;
            i = i == 0 ? eloop->cap - 1 : i - 1;
            exb_assert_h(memcmp(events + idx, eloop->events + i, sizeof(struct exb_event)) == 0, "");
        }
    #endif
    return EXB_OK;
}

static int exb_eloop_append_delayed(struct exb_eloop *eloop, struct exb_event ev, int ms, int tolerate_preexec);

static int exb_eloop_flush_tasks(struct exb_eloop *eloop) {
    int err = EXB_OK;
    if (eloop->ntasks > 0)
        err = exb_threadpool_push_tasks_many(eloop->threadpool, eloop->task_buffer, eloop->ntasks);
    if (err != EXB_OK)
        return err;
    eloop->ntasks = 0;
    eloop->tasks_flush_min_delay_ms = 1000;
    return EXB_OK;
}

//untested
static int exb_eloop_push_task(struct exb_eloop *eloop, struct exb_task task, int acceptable_delay_ms) {
    int err;
    if (  eloop->ntasks >= EXB_ELOOP_TASK_BUFFER_COUNT && 
        ((err = exb_eloop_flush_tasks(eloop)) != EXB_OK))
    {
        return err;
    }
    eloop->task_buffer[eloop->ntasks] = task;
    if (eloop->ntasks == 0                           || 
        acceptable_delay_ms < eloop->tasks_flush_min_delay_ms) 
    {
        eloop->tasks_flush_min_delay_ms = acceptable_delay_ms;
        struct exb_event flush_ev = exb_eloop_make_eloop_event(eloop, EXB_ELOOP_FLUSH);
        exb_eloop_append_delayed(eloop, flush_ev, acceptable_delay_ms, 1);
    }
    eloop->ntasks++;
        
    return EXB_OK;
}


/*Threadsafe append event*/
static int exb_eloop_ts_append(struct exb_eloop *eloop, struct exb_event event) {
    int rv = exb_ts_event_queue_append(&eloop->tsq, event);
    return rv;
}
/*Threadsafe pop event*/
static int exb_eloop_ts_pop(struct exb_eloop *eloop, struct exb_event *event_out) {
    int rv = exb_ts_event_queue_pop_next(&eloop->tsq, event_out);
    return rv;
}
static int exb_eloop_ts_pop_many(struct exb_eloop *eloop, struct exb_event *events_out, int *nevents, int max_events) {
    int rv = exb_ts_event_queue_pop_many(&eloop->tsq, events_out, nevents, max_events);
    return rv;
}

//copies event, [eventually calls ev->destroy()]
static int exb_eloop_append_delayed(struct exb_eloop *eloop, struct exb_event ev, int ms, int tolerate_preexec) {
    return exb_eloop_d_push(eloop, ev, exb_timestamp_add_usec(eloop->current_timestamp, ms * 1000), tolerate_preexec);
}

static int exb_eloop_fatal(struct exb_eloop *eloop, struct exb_error err) {
    abort();
}
//thread safe
static int exb_eloop_stop(struct exb_eloop *eloop) {
    struct exb_event ev;
    eloop->do_stop = 1;
    return EXB_OK;
}

//recieves events from other threads
static int exb_eloop_receive(struct exb_eloop *eloop) {
    struct exb_event events[EXB_ELOOP_TMP_EVENTS_SZ];
    int nevents = 0;
    #define MAX_BATCHES 3
    int rv = EXB_OK;
    for (int i=0;
         (i == 0 || nevents == EXB_ELOOP_TMP_EVENTS_SZ) && i<MAX_BATCHES;
          i++) 
    {
        int rv = exb_eloop_ts_pop_many(eloop, events, &nevents, EXB_ELOOP_TMP_EVENTS_SZ);
        if (rv != EXB_OK) {
            if (rv == EXB_OUT_OF_RANGE_ERR) {
                return EXB_OK;
            }
            return rv;
        }
        rv = exb_eloop_append_many(eloop, events, nevents);
        if (rv != EXB_OK) {
            exb_assert_h(0, ""); //this 'll ruin order
            for (int i=0; i<nevents; i++) {
                struct exb_event *ev = events + i;
                int err = exb_eloop_ts_append(eloop, *ev);
                if (err != EXB_OK) {
                    exb_eloop_fatal(eloop, exb_make_error(err));
                }
            }
            return rv;
        }
    }
    
    return EXB_OK;
}

static struct exb_error exb_eloop_run(struct exb_eloop *eloop) {
    #define EXB_ELOOP_NPROCESS_OFFLOAD 64
    int nprocessed = 0;

    eloop->current_timestamp = exb_timestamp_now();
    while (!eloop->do_stop) {
        struct exb_event ev;
        
         //TODO: [scheduling] sometimes ignore timed events and pop from array anyways to ensure progress

        int rv = EXB_OUT_OF_RANGE_ERR;
        for (int i = 0; i < 16; i++) {
            rv = exb_eloop_q_pop_next(eloop, &ev);
            if (rv != EXB_OK)
                break;
            exb_assert_s(!!ev.handle, "ev.handle == NULL");
            ev.handle(ev);
        }
        if (rv != EXB_OK && rv != EXB_OUT_OF_RANGE_ERR) {
            //serious error
            struct exb_error err = exb_make_error(rv);
            return err;
        }
        
        int dlen = exb_eloop_d_len(eloop);
        for (int i = 0; i < dlen; i++) {
            rv = exb_eloop_d_pop_next(eloop, &ev);
            if (rv != EXB_OK)
                break;
            exb_assert_s(!!ev.handle, "ev.handle == NULL");
            ev.handle(ev);
        }
        
        if (rv != EXB_OK && rv != EXB_OUT_OF_RANGE_ERR) {
            //serious error
            struct exb_error err = exb_make_error(rv);
            return err;
        }
        exb_eloop_receive(eloop);
        if (exb_eloop_q_len(eloop) == 0) {
            exb_sleep_until_timestamp_ex(eloop->current_timestamp, exb_eloop_next_delayed_event_timestamp(eloop));
        }
        eloop->current_timestamp = exb_timestamp_now();
    }
    
    return exb_make_error(EXB_OK);
}

//TODO: optimize to recycle or use custom allocator
static struct exb_error exb_eloop_alloc_buffer(struct exb_eloop *eloop, size_t size, char **buff_out, int *cap_out) {
    void *p;
    size_t cap;
    if (exb_buffer_recycle_list_pop(eloop->exb, &eloop->buff_cyc, size, &p, &cap) == EXB_OK) {
        *buff_out = p;
        *cap_out = cap;
        return exb_make_error(EXB_OK);
    }
    *buff_out = exb_malloc(eloop->exb, size);
    if (!*buff_out) {
        return exb_make_error(EXB_NOMEM_ERR);
    }
    *cap_out = size;
    return exb_make_error(EXB_OK);
}
static struct exb_error exb_eloop_realloc_buffer(struct exb_eloop *eloop, char *buff, size_t new_size, char **buff_out, int *cap_out) {
    char *new_buff = exb_realloc(eloop->exb, buff, new_size);
    if (!new_buff) {
        return exb_make_error(EXB_NOMEM_ERR);
    }
    *buff_out = new_buff;
    *cap_out = new_size;
    return exb_make_error(EXB_OK);
}
static void exb_eloop_release_buffer(struct exb_eloop *eloop, char *buff, size_t capacity) {
    if (exb_buffer_recycle_list_push(eloop->exb, &eloop->buff_cyc, buff, capacity) == EXB_OK)
        return;
    exb_free(eloop->exb, buff);
}

#endif// ELOOP_H
