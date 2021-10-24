#ifndef EXB_THREAD_H
#define EXB_THREAD_H
#include <pthread.h>
#include <string.h>
#include "exb.h"
struct exb_threadpool;

int exb_hw_cpu_count();
int exb_hw_bind_to_core(int core_id);
int exb_hw_bind_not_to_core(int core_id);
int exb_hw_thread_sched_important();
int exb_hw_thread_sched_background();


struct exb_thread {
    int bind_cpu;
    int tid;
    void *data;
    
    pthread_t thread;
};

static int exb_thread_join(struct exb_thread *thread) {
    void *r;
    int rv = pthread_join(thread->thread, &r);
    return rv == 0 ? EXB_OK : EXB_THREAD_ERROR;
}

static int exb_thread_new(struct exb *exb_ref, int tid, void *(*run)(void *), void *data, struct exb_thread **new_thread) {
    void *p = exb_malloc(exb_ref, sizeof(struct exb_thread));
    if (!p)
        return EXB_NOMEM_ERR;
    struct exb_thread *t = p;
    memset(t, 0, sizeof(*t));
    t->tid = tid;
    t->data = data;
    t->bind_cpu = tid % exb_hw_cpu_count();
    exb_assert_h(t->bind_cpu >= 0, "invalid thread id");
    int rv = pthread_create(&t->thread, NULL, run, t);
    if (rv != 0) {
        exb_free(exb_ref, t);
        return EXB_THREAD_ERROR;
    }
    *new_thread = t;
    return EXB_OK;
}
static void exb_thread_destroy(struct exb_thread *thread, struct exb *exb_ref) {
    /*this doesn't attempt to cancel the thread*/
    /*it is assumed that it is no longer executing*/
    exb_free(exb_ref, thread);
}
static int exb_thread_cancel_and_destroy(struct exb_thread *thread, struct exb *exb_ref) {
    pthread_cancel(thread->thread);
    exb_thread_destroy(thread, exb_ref);
    return EXB_OK;
}

#endif // EXB_THREAD_H