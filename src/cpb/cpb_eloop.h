#ifndef ELOOP_H
#define ELOOP_H
/*
Event loop
*/
#include "cpb.h"
#include "string.h"
#include "cpb_utils.h"
#include "cpb_task.h"
#include "cpb_ts_event_queue.h"
#include "cpb_event.h"
#include "cpb_threadpool.h"
#include "cpb_buffer_recycle_list.h"
#define ELOOP_SLEEP_TIME 2
#define CPB_ELOOP_TMP_EVENTS_SZ 256
#define CPB_ELOOP_TASK_BUFFER_COUNT 256
#define CPB_ELOOP_DELAYED_MAX 4 // 4 : 1 ratio

struct cpb_event; //fwd
struct cpb_threadpool;

struct cpb_delayed_event {
    double due; //unix time + delay
    struct cpb_event event;
    unsigned char tolerate_preexec;
};
struct cpb_delayed_event_node {
    struct cpb_delayed_event cur;
    struct cpb_delayed_event_node *next;
};

enum cpb_eloop_event_cmd {
    CPB_ELOOP_FLUSH,
};

struct cpb_eloop;
static int cpb_eloop_flush_tasks(struct cpb_eloop *eloop);

static void cpb_eloop_handle_eloop_event(struct cpb_event ev) {
    struct cpb_eloop *eloop = ev.msg.u.iip.argp;
    int cmd = ev.msg.u.iip.arg1;
    if (cmd == CPB_ELOOP_FLUSH) {
        cpb_eloop_flush_tasks(eloop);
    }
    else {
        cpb_assert_h(0, "unknown cmd");
    }
}
static void cpb_eloop_destroy_eloop_event(struct cpb_event ev) {

}


static struct cpb_event_handler_itable cpb_eloop_events_itable = {
    .handle = cpb_eloop_handle_eloop_event,
    .destroy = cpb_eloop_destroy_eloop_event
};
static struct cpb_event cpb_eloop_make_eloop_event(struct cpb_eloop *eloop, enum cpb_eloop_event_cmd cmd) {
    struct cpb_event ev;
    ev.itable = &cpb_eloop_events_itable;
    ev.msg.u.iip.arg1 = cmd;
    ev.msg.u.iip.argp = eloop;
    ev.msg.u.iip.arg2 = 0;
    return ev;
}



struct cpb_eloop {
    struct cpb *cpb; //not owned, must outlive
    struct cpb_threadpool *threadpool; //not owned, must outlive
    struct cpb_ts_event_queue tsq;
    struct cpb_buffer_recycle_list buff_cyc;
    
    unsigned ev_id; //counter

    struct cpb_delayed_event_node* devents;
    int devents_len;
    int ndelayed_processed;

    //this is done to collect a number of tasks before sending them to the threadpool
    struct cpb_task task_buffer[CPB_ELOOP_TASK_BUFFER_COUNT];
    int ntasks;
    int tasks_flush_min_delay_ms;
    double tasks_flush_ts;
    

    struct cpb_event* events;
    int head;
    int tail; //tail == head means full, tail == head - 1 means full
    int cap;
};

//_d_ functions are for delayed events which are kept in a linked list
//_q_ functions are for regular events which are kept in an array as a queue

static int cpb_eloop_d_push(struct cpb_eloop *eloop, struct cpb_event event, double due, int tolerate_prexec) {
    void *p = cpb_malloc(eloop->cpb, sizeof(struct cpb_delayed_event_node));
    if (!p)
        return CPB_NOMEM_ERR;
    struct cpb_delayed_event_node *node = p;
    node->cur.event = event;
    node->cur.due = due;
    node->cur.tolerate_preexec = tolerate_prexec;
    if (eloop->devents == NULL || (eloop->devents->cur.due > node->cur.due)) {
        node->next = eloop->devents;
        eloop->devents = node;
    }
    else {
        struct cpb_delayed_event_node *at = eloop->devents;
        while (at->next && at->next->cur.due < node->cur.due)
            at = at->next;
        node->next = at->next;
        at->next = node;
    }
    eloop->devents_len++;
    return CPB_OK;
}
static double cpb_eloop_time_until_due(struct cpb_delayed_event_node *ev) {
    double cur_time = cpb_time();
    if (ev->cur.due <= cur_time) {
        return 0;
    }
    return ev->cur.due - cur_time;
}
static double cpb_eloop_sleep_till(struct cpb_eloop *eloop) {
    struct cpb_delayed_event_node *at = eloop->devents;
    const double min = (ELOOP_SLEEP_TIME / 1024);
    if (!at) {
        return min;
    }
    double till = cpb_eloop_time_until_due(at);
    return till > min ? min : till;
}

static int cpb_eloop_q_len(struct cpb_eloop *eloop) {
    if (eloop->tail >= eloop->head) {
        return eloop->tail - eloop->head;
    }
    return eloop->cap - eloop->head + eloop->tail;
}

static int cpb_eloop_q_pop_next(struct cpb_eloop *eloop, struct cpb_event *ev_out) {
    if (cpb_eloop_q_len(eloop) == 0)
        return CPB_OUT_OF_RANGE_ERR;
    struct cpb_event ev = eloop->events[eloop->head];
    eloop->head++;
    if (eloop->head >= eloop->cap) //eloop->head %= eloop->cap;
        eloop->head = 0;
    *ev_out = ev;
    return CPB_OK;
}
static int cpb_eloop_d_pop_next(struct cpb_eloop *eloop, struct cpb_event *ev_out) {
    if (eloop->devents_len < 1)
        return CPB_OUT_OF_RANGE_ERR;
    
    struct cpb_delayed_event_node *next = eloop->devents->next;
    struct cpb_delayed_event_node *head = eloop->devents;
    *ev_out = head->cur.event;
    cpb_free(eloop->cpb, head);
    eloop->devents = next;
    eloop->devents_len--;
    return CPB_OK;
}
//try to pop an event that doesn't have a problem with being executed prematurely
static int cpb_eloop_d_pop_next_premature(struct cpb_eloop *eloop, struct cpb_event *ev_out) {
    if (eloop->devents_len < 1)
        return CPB_OUT_OF_RANGE_ERR;
    
    struct cpb_delayed_event_node *next = eloop->devents->next;
    struct cpb_delayed_event_node *head = eloop->devents;
    if (!head->cur.tolerate_preexec) { 
        /*TODO, find ones other than head*/
        /*Also, we can have multiple linked lists for this condition*/
        return CPB_OUT_OF_RANGE_ERR;
    }
    *ev_out = head->cur.event;
    cpb_free(eloop->cpb, head);
    eloop->devents = next;
    eloop->devents_len--;
    return CPB_OK;
}

static int cpb_eloop_d_len(struct cpb_eloop *eloop) {
    return eloop->devents_len;
}
static struct cpb_delayed_event *cpb_eloop_d_peek_next(struct cpb_eloop *eloop) {
    if (cpb_eloop_d_len(eloop) == 0)
        return NULL;
    return &eloop->devents->cur;
}
static int cpb_eloop_pop_next(struct cpb_eloop *eloop, double cpb_cur_time, struct cpb_event *ev_out) {
    struct cpb_delayed_event *dev = cpb_eloop_d_peek_next(eloop);
    if (eloop->ndelayed_processed < CPB_ELOOP_DELAYED_MAX && dev != NULL && dev->due <= cpb_cur_time) {
        eloop->ndelayed_processed++;
        return cpb_eloop_d_pop_next(eloop, ev_out);
    }
    eloop->ndelayed_processed = 0;
    return cpb_eloop_q_pop_next(eloop, ev_out);
}

static int cpb_eloop_len(struct cpb_eloop *eloop) {
    return cpb_eloop_q_len(eloop) + cpb_eloop_d_len(eloop);
}

static int cpb_eloop_resize(struct cpb_eloop *eloop, int sz);
static int cpb_eloop_init(struct cpb_eloop *eloop, struct cpb* cpb_ref, struct cpb_threadpool *tp, int sz) {
    memset(eloop, 0, sizeof *eloop);
    eloop->threadpool = tp;
    eloop->devents_len = 0;
    eloop->cpb = cpb_ref;
    eloop->ndelayed_processed = 0;

    eloop->ntasks = 0;
    eloop->tasks_flush_ts = 0.0;
    eloop->tasks_flush_min_delay_ms = 0;

    int rv;
    if ((rv = cpb_buffer_recycle_list_init(cpb_ref, &eloop->buff_cyc)) != CPB_OK) {
        return rv;
    }
    rv = cpb_ts_event_queue_init(&eloop->tsq, cpb_ref, 16);
    if (rv != CPB_OK) {
        cpb_buffer_recycle_list_deinit(cpb_ref, &eloop->buff_cyc);
        return rv;
    }
    if (sz != 0) {
        rv = cpb_eloop_resize(eloop, sz);
        if (rv != CPB_OK) {
            cpb_buffer_recycle_list_deinit(cpb_ref, &eloop->buff_cyc);
            cpb_ts_event_queue_deinit(&eloop->tsq);
            return rv;
        }
    }
    return CPB_OK;
}
static int cpb_eloop_deinit(struct cpb_eloop *eloop) {
    //TODO destroy pending events
    fprintf(stderr, "freeing eloop %p\n", eloop);
    cpb_buffer_recycle_list_deinit(eloop->cpb, &eloop->buff_cyc);
    cpb_ts_event_queue_deinit(&eloop->tsq);
    cpb_free(eloop->cpb, eloop->events);
    return CPB_OK;
}

static int cpb_eloop_resize(struct cpb_eloop *eloop, int sz) {
    cpb_assert_h(!!eloop->cpb, "");
    void *p = cpb_malloc(eloop->cpb, sizeof(struct cpb_event) * sz);

    if (!p) {
        return CPB_NOMEM_ERR;
    }
    //this can be optimized, see also taskqueue
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
    if (cpb_eloop_q_len(eloop) >= eloop->cap - 1) {
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

//see also thread_pool_append_many
static int cpb_eloop_append_many(struct cpb_eloop *eloop, struct cpb_event *events, int nevents) {
    
    if ((cpb_eloop_q_len(eloop) + nevents) >= eloop->cap - 1) {
        int nsz = eloop->cap ? eloop->cap * 2 : 4;
        while ((cpb_eloop_q_len(eloop) + nevents) >= nsz - 1)
            nsz *= 2;
        int rv = cpb_eloop_resize(eloop, nsz);
        if (rv != CPB_OK)
            return rv;
    }
    cpb_assert_h((cpb_eloop_q_len(eloop) + nevents) < eloop->cap - 1, "");
    eloop->ev_id += nevents;

    if (eloop->head > eloop->tail) {
        //example: cap=8
        //         []
        //indices: 0  1  2  3  4  5  6  7
        //               ^tail       ^head
        //               ^^^^^^^^^^
        //               at tail
        // no at head
        cpb_assert_h(eloop->tail + nevents < eloop->head, "");
        memcpy(eloop->events + eloop->tail, events, nevents * sizeof(struct cpb_event));
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
        memcpy(eloop->events + eloop->tail, events, at_tail * sizeof(struct cpb_event));
        int remainder = nevents - at_tail;
        if (remainder > 0) {
            cpb_assert_h(eloop->head > remainder, "");
            //A STUPID BUG WAS FIXED HERE!
            memcpy(eloop->events, events + at_tail, remainder * sizeof(struct cpb_event));
            eloop->tail = remainder;
        }
        else {
            cpb_assert_h(remainder == 0, "");
            eloop->tail += at_tail;
            if (eloop->tail >= eloop->cap)
                eloop->tail = 0;
        }
    }
    #if defined(CPB_ASSERTS) && 0 
        //TODO: MOVE TO A TEST SUITE
        int i = eloop->tail;
        for (int idx = nevents; idx > 0;) {
            cpb_assert_h(i != eloop->head, "");
            idx--;
            i = i == 0 ? eloop->cap - 1 : i - 1;
            cpb_assert_h(memcmp(events + idx, eloop->events + i, sizeof(struct cpb_event)) == 0, "");
        }
    #endif
    return CPB_OK;
}

static int cpb_eloop_append_delayed(struct cpb_eloop *eloop, struct cpb_event ev, int ms, int tolerate_preexec);

static int cpb_eloop_flush_tasks(struct cpb_eloop *eloop) {
    int err = CPB_OK;
    if (eloop->ntasks > 0)
        err = cpb_threadpool_push_tasks_many(eloop->threadpool, eloop->task_buffer, eloop->ntasks);
    if (err != CPB_OK)
        return err;
    eloop->ntasks = 0;
    eloop->tasks_flush_ts = 0.0;
    eloop->tasks_flush_min_delay_ms = 1000;
}
static int cpb_eloop_push_task(struct cpb_eloop *eloop, struct cpb_task task, int acceptable_delay_ms) {
    int err;
    if (  eloop->ntasks >= CPB_ELOOP_TASK_BUFFER_COUNT && 
        ((err = cpb_eloop_flush_tasks(eloop)) != CPB_OK))
    {
        return err;
    }
    eloop->task_buffer[eloop->ntasks] = task;
    if (eloop->ntasks == 0                           || 
        acceptable_delay_ms < eloop->tasks_flush_min_delay_ms) 
    {
        eloop->tasks_flush_min_delay_ms = acceptable_delay_ms;
        struct cpb_event flush_ev = cpb_eloop_make_eloop_event(eloop, CPB_ELOOP_FLUSH);
        cpb_eloop_append_delayed(eloop, flush_ev, acceptable_delay_ms, 1);
    }
    eloop->ntasks++;
        
    return CPB_OK;
}


/*Threadsafe append event*/
static int cpb_eloop_ts_append(struct cpb_eloop *eloop, struct cpb_event event) {
    dp_register_event(__FUNCTION__);
    int rv = cpb_ts_event_queue_append(&eloop->tsq, event);
    dp_end_event(__FUNCTION__);
    return rv;
}
/*Threadsafe pop event*/
static int cpb_eloop_ts_pop(struct cpb_eloop *eloop, struct cpb_event *event_out) {
    dp_register_event(__FUNCTION__);
    int rv = cpb_ts_event_queue_pop_next(&eloop->tsq, event_out);
    dp_end_event(__FUNCTION__);
    return rv;
}
static int cpb_eloop_ts_pop_many(struct cpb_eloop *eloop, struct cpb_event *events_out, int *nevents, int max_events) {
    dp_register_event(__FUNCTION__);
    int rv = cpb_ts_event_queue_pop_many(&eloop->tsq, events_out, nevents, max_events);
    dp_end_event(__FUNCTION__);
    return rv;
}

//copies event, [eventually calls ev->destroy()]
static int cpb_eloop_append_delayed(struct cpb_eloop *eloop, struct cpb_event ev, int ms, int tolerate_preexec) {
    return cpb_eloop_d_push(eloop, ev, cpb_time() + (ms / 1024.0), tolerate_preexec);
}

static int cpb_eloop_fatal(struct cpb_eloop *eloop, struct cpb_error err) {
    abort();
}

static int cpb_eloop_offload_recieve(struct cpb_eloop *eloop) {
    /*TODO: pop many*/
    struct cpb_event ev;
    
    struct cpb_event events[CPB_ELOOP_TMP_EVENTS_SZ];
    int nevents;

    int rv = cpb_eloop_ts_pop_many(eloop, events, &nevents, CPB_ELOOP_TMP_EVENTS_SZ);
    if (rv != CPB_OK) {
        if (rv == CPB_OUT_OF_RANGE_ERR) {
            return CPB_OK;
        }
        return rv;
    }
    rv = cpb_eloop_append_many(eloop, events, nevents);
    if (rv != CPB_OK) {
        cpb_assert_h(0, ""); //this 'll ruin order
        for (int i=0; i<nevents; i++) {
            struct cpb_event *ev = events + i;
            int err = cpb_eloop_ts_append(eloop, *ev);
            if (err != CPB_OK) {
                cpb_eloop_fatal(eloop, cpb_make_error(err));
            }
        }
        return rv;
    }
    
    return CPB_OK;
}

static struct cpb_error cpb_eloop_run(struct cpb_eloop *eloop) {
    //cpb_hw_bind_to_core(0);

    int ineffective_spins = 0;
    #define CPB_ELOOP_NPROCESS_OFFLOAD 64
    int nprocessed = 0;

    double cur_time = cpb_time();
    while (1) {
        struct cpb_event ev;
        
         //TODO: [scheduling] sometimes ignore timed events and pop from array anyways to ensure progress
        dp_register_event("eloop_pop_next");
        int rv = cpb_eloop_pop_next(eloop, cur_time, &ev);
        
        dp_end_event("eloop_pop_next");
    again:
        if (rv == CPB_OK) {
            //handle event
            ev.itable->handle(ev);
            ev.itable->destroy(ev);
            if (nprocessed++ > CPB_ELOOP_NPROCESS_OFFLOAD) {
                dp_register_event("eloop_offload");
                cpb_eloop_offload_recieve(eloop);
                dp_end_event("eloop_offload");
                nprocessed = 0;
            }
        }
        else {
            if (rv != CPB_OUT_OF_RANGE_ERR) {
                //serious error
                struct cpb_error err = cpb_make_error(rv);
                return err;
            }
            cur_time = cpb_time();
            dp_register_event("eloop_pop_next");
            rv = cpb_eloop_pop_next(eloop, cur_time, &ev);
            dp_end_event("eloop_pop_next");
            if (rv == CPB_OK) {
                goto again;
            }
            else if (rv != CPB_OUT_OF_RANGE_ERR) {
                //serious error
                struct cpb_error err = cpb_make_error(rv);
                return err;
            }
            #if 0
            dp_register_event("eloop_pop_next_p");
            rv = cpb_eloop_d_pop_next_premature(eloop, &ev);
            dp_end_event("eloop_pop_next_p");
            if (rv == CPB_OK) {
                goto again;
            }
            else if (rv != CPB_OUT_OF_RANGE_ERR) {
                //serious error
                struct cpb_error err = cpb_make_error(rv);
                return err;
            }
            #endif
        
            dp_register_event("eloop_sleep");
            cpb_sleep(cpb_eloop_sleep_till(eloop) * 1024);
            dp_end_event("eloop_sleep");
            dp_register_event("eloop_offload_2");
            cpb_eloop_offload_recieve(eloop);
            dp_end_event("eloop_offload_2");
        }
    }
    
    return cpb_make_error(CPB_OK);
}

//TODO: optimize to recycle or use custom allocator
static struct cpb_error cpb_eloop_alloc_buffer(struct cpb_eloop *eloop, size_t size, char **buff_out, int *cap_out) {
    void *p;
    size_t cap;
    if (cpb_buffer_recycle_list_pop(eloop->cpb, &eloop->buff_cyc, size, &p, &cap) == CPB_OK) {
        *buff_out = p;
        *cap_out = cap;
        return cpb_make_error(CPB_OK);
    }
    *buff_out = cpb_malloc(eloop->cpb, size);
    if (!*buff_out) {
        return cpb_make_error(CPB_NOMEM_ERR);
    }
    *cap_out = size;
    return cpb_make_error(CPB_OK);
}
static struct cpb_error cpb_eloop_realloc_buffer(struct cpb_eloop *eloop, char *buff, size_t new_size, char **buff_out, int *cap_out) {
    char *new_buff = cpb_realloc(eloop->cpb, buff, new_size);
    if (!new_buff) {
        return cpb_make_error(CPB_NOMEM_ERR);
    }
    *buff_out = new_buff;
    *cap_out = new_size;
    return cpb_make_error(CPB_OK);
}
static void cpb_eloop_release_buffer(struct cpb_eloop *eloop, char *buff, size_t capacity) {
    if (cpb_buffer_recycle_list_push(eloop->cpb, &eloop->buff_cyc, buff, capacity) == CPB_OK)
        return;
    cpb_free(eloop->cpb, buff);
}

#endif// ELOOP_H