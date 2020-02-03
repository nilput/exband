#ifndef CPB_THREAD_H
#define CPB_THREAD_H
#include <pthread.h>
#include "cpb.h"
struct cpb_threadpool;
struct cpb_thread {
    //can be null if thread is not part of a threadpool
    struct cpb_threadpool *tp; //not owned, must outlive
    int tid;
    void *data;

    pthread_t thread;
};

static int cpb_thread_join(struct cpb_thread *thread) {
    void *r;
    int rv = pthread_join(thread->thread, &r);
    return rv == 0 ? CPB_OK : CPB_THREAD_ERROR;
}

static int cpb_thread_new(struct cpb *cpb_ref, int tid, struct cpb_threadpool *tp, void *(*run)(void *), void *data, struct cpb_thread **new_thread) {
    void *p = cpb_malloc(cpb_ref, sizeof(struct cpb_thread));
    if (!p)
        return CPB_NOMEM_ERR;
    struct cpb_thread *t = p;
    memset(t, 0, sizeof(*t));
    t->tid = tid;
    t->tp = tp;
    t->data = data;
    int rv = pthread_create(&t->thread, NULL, run, t);
    if (rv != 0) {
        cpb_free(cpb_ref, t);
        return CPB_THREAD_ERROR;
    }
    *new_thread = t;
    return CPB_OK;
}
static void cpb_thread_destroy(struct cpb_thread *thread, struct cpb *cpb_ref) {
    /*this doesn't attempt to cancel the thread*/
    /*it is assumed that it is no longer executing*/
    cpb_free(cpb_ref, thread);
}
static int cpb_thread_cancel_and_destroy(struct cpb_thread *thread, struct cpb *cpb_ref) {
    pthread_cancel(thread->thread);
    cpb_thread_destroy(thread, cpb_ref);
    return CPB_OK;
}

#endif // CPB_THREAD_H