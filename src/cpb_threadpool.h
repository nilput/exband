#ifndef CPB_THREADPOOL_H
#define CPB_THREADPOOL_H
#include <string.h>
#include <stdio.h>

#include <pthread.h>

#include "cpb.h"
#include "cpb_errors.h"
#include "cpb_utils.h"

struct cpb_threadpool;
struct cpb_thread {
    struct cpb_threadpool *tp; //not owned, must outlive
    int tid;

    pthread_t thread;
};
struct cpb_taskqueue {
    struct cpb *cpb; //not owned, must outlive
    struct cpb_task *tasks;
    int head;
    int tail; //tail == head means full, tail == head - 1 means full
    int cap;
};
struct cpb_threadpool {
    int tid;
    pthread_mutex_t tp_mtx;

    pthread_mutex_t tp_cnd_mtx;
    pthread_cond_t tp_cnd;

    struct cpb *cpb; //not owned, must outlive
    struct cpb_thread **threads; /*array of pointers*/
    int nthreads;

    struct cpb_taskqueue taskq;
};


struct cpb_task {
    struct cpb_error err;
    struct cpb_msg msg;
    void (*run)(struct cpb_thread *thread, struct cpb_task *task);
};


static int cpb_taskqueue_deinit(struct cpb_taskqueue *tq); //fwd
static int cpb_taskqueue_init(struct cpb_taskqueue *tq, struct cpb* cpb_ref, int sz);
static void cpb_thread_destroy(struct cpb_thread *thread);

static int cpb_threadpool_init(struct cpb_threadpool *tp, struct cpb* cpb_ref) {
    tp->tid = 0;
    tp->nthreads = 0;
    tp->threads = NULL;
    int err  = CPB_OK;
    if (pthread_mutex_init(&tp->tp_mtx, NULL) != 0) {
        err = CPB_MUTEX_ERROR;
    }
    else {
        err = cpb_taskqueue_init(&tp->taskq, cpb_ref, 128);
    }
    return err;
}



static void cpb_threadpool_deinit(struct cpb_threadpool *tp) {
    for (int i=0; i<tp->nthreads; i++) {
        /*ASSUMES THREAD STOPPED EXECUTING*/
        cpb_thread_destroy(tp->threads[i]);
    }
    cpb_free(tp->cpb, tp->threads);
    cpb_taskqueue_deinit(&tp->taskq);
}

static int cpb_taskqueue_pop_next(struct cpb_taskqueue *tq, struct cpb_task *task_out);
static int cpb_taskqueue_append(struct cpb_taskqueue *tq, struct cpb_task task);
static int cpb_taskqueue_len(struct cpb_taskqueue *tq);

/*Must be threadsafe*/
/*TODO: optimize, use a CAS/LLSC algorithm with a fallback on mutexes*/
static int cpb_threadpool_push_task(struct cpb_threadpool *tp, struct cpb_task task) {
    if (pthread_mutex_lock(&tp->tp_mtx) != 0) {
        return CPB_MUTEX_LOCK_ERROR;
    }
    int err = CPB_OK;
    err = cpb_taskqueue_append(&tp->taskq, task);
ret:
    pthread_mutex_unlock(&tp->tp_mtx);
    pthread_cond_signal(&tp->tp_cnd);
    return err;
}
/*Must be threadsafe*/
/*TODO: optimize, use a CAS/LLSC algorithm with a fallback on mutexes*/
static int cpb_threadpool_pop_task(struct cpb_threadpool *tp, struct cpb_task *task_out) {
    if (pthread_mutex_lock(&tp->tp_mtx) != 0) {
        return CPB_MUTEX_LOCK_ERROR;
    }
    int err = CPB_OK;
    err = cpb_taskqueue_pop_next(&tp->taskq, task_out);
ret:
    pthread_mutex_unlock(&tp->tp_mtx);
    return err;
}

/*Must be threadsafe*/
/*Waits until new tasks are available (this can be done way better)*/
static int cpb_threadpool_wait_for_work(struct cpb_threadpool *tp) {
    int rv = pthread_mutex_lock(&tp->tp_cnd_mtx);
    if (rv != 0) {
        return CPB_MUTEX_LOCK_ERROR;
    }
    while (cpb_taskqueue_len(&tp->taskq) == 0) {
        rv = pthread_cond_wait(&tp->tp_cnd, &tp->tp_cnd_mtx);
        if (rv != 0) {
            return CPB_MUTEX_LOCK_ERROR;
        }
    }
    pthread_mutex_unlock(&tp->tp_cnd_mtx);
    return CPB_OK;
}

static void  *cpb_thread_run(void *arg) {
    struct cpb_thread *t = arg;
    struct cpb_task current_task;
    while (1) {
        int rv = cpb_threadpool_pop_task(t->tp, &current_task);
        if (rv != CPB_OK) { 
            cpb_threadpool_wait_for_work(t->tp);
            continue;
        }
        current_task.run(t, &current_task);
    }
    return arg;
}
static int cpb_thread_cancel_and_destroy(struct cpb_thread *thread);


static int cpb_thread_new(struct cpb_threadpool *tp, int tid, struct cpb_thread **new_thread) {
    void *p = cpb_malloc(tp->cpb, sizeof(struct cpb_thread));
    if (!p)
        return CPB_NOMEM_ERR;
    struct cpb_thread *t = p;
    memset(t, 0, sizeof(*t));
    t->tid = tid;
    t->tp = tp;
    int rv = pthread_create(&t->thread, NULL, cpb_thread_run, t);
    if (rv != 0) {
        cpb_free(tp->cpb, t);
        return CPB_THREAD_ERROR;
    }
    *new_thread = t;
    return CPB_OK;
}
static void cpb_thread_destroy(struct cpb_thread *thread) {
    /*this doesn't attempt to cancel the thread*/
    /*it is assumed that it is no longer executing*/
    cpb_free(thread->tp->cpb, thread);
}
static int cpb_thread_cancel_and_destroy(struct cpb_thread *thread) {
    pthread_cancel(thread->thread);
    cpb_thread_destroy(thread);
    return CPB_OK;
}

static int cpb_threadpool_set_nthreads(struct cpb_threadpool *tp, int nthreads) {
    if (pthread_mutex_lock(&tp->tp_mtx) != 0) {
        return CPB_MUTEX_LOCK_ERROR;
    }
    int err = CPB_OK;
    int old_thread_count = tp->nthreads;

    if (nthreads == old_thread_count) {
        err = CPB_OK;
        goto ret;
    }
    else if (nthreads < old_thread_count) {
        err = CPB_UNSUPPORTED;;
        goto ret;
    }
    cpb_assert_s(nthreads > old_thread_count, "");

    {
        void *p = cpb_realloc(tp->cpb, tp->threads, sizeof(struct cpb_thread) * nthreads);
        if (!p) {
            err = CPB_NOMEM_ERR;
            goto ret;
        }
        tp->threads = p;
    }
    
    for (int i=old_thread_count; i<nthreads; i++) {
        int rv = cpb_thread_new(tp, tp->tid, &tp->threads[i]);
        if (rv != CPB_OK) {
            for (int j=i-1; j>=old_thread_count; j--) {
                cpb_thread_cancel_and_destroy(tp->threads[j]);
                tp->tid--;
                err = rv;
                goto ret;
            }
        }
        tp->tid++;
    }
    tp->nthreads = nthreads;

ret:
    pthread_mutex_unlock(&tp->tp_mtx);
    return err;
}

static int cpb_taskqueue_len(struct cpb_taskqueue *tq) {
    if (tq->tail >= tq->head) {
        return tq->tail - tq->head;
    }
    return tq->cap - tq->head + tq->tail;
}
static int cpb_taskqueue_resize(struct cpb_taskqueue *tq, int sz);
static int cpb_taskqueue_append(struct cpb_taskqueue *tq, struct cpb_task task) {
    if (cpb_taskqueue_len(tq) == tq->cap - 1) {
        int nsz = tq->cap * 2;
        int rv = cpb_taskqueue_resize(tq, nsz > 0 ? nsz : 4);
        if (rv != CPB_OK)
            return rv;
    }
    tq->tasks[tq->tail] = task;
    tq->tail++;
    if (tq->tail >= tq->cap) //tq->tail %= tq->cap;
        tq->tail = 0;
    return CPB_OK;
}
static int cpb_taskqueue_resize(struct cpb_taskqueue *tq, int sz) {
    cpb_assert_h(!!tq->cpb, "");
    void *p = cpb_malloc(tq->cpb, sizeof(struct cpb_task) * sz);

    if (!p) {
        return CPB_NOMEM_ERR;
    }
    //this can be optimized, see also eloop
    struct cpb_task *tasks = p;
    int idx = 0;
    for (int i=tq->head; ; i++) {
        if (i >= tq->cap) 
            i = 0;
        if (i == tq->tail)
            break;
        if (idx >= (sz - 1)) {
            break; //some events were lost because size is less than needed!
        }
        tasks[idx] = tq->tasks[i];
        idx++;
    }
    cpb_free(tq->cpb, tq->tasks);
    int prev_len = cpb_taskqueue_len(tq);
    tq->tasks = p;
    tq->head = 0;
    tq->tail = idx;
    tq->cap = sz;
    int new_len = cpb_taskqueue_len(tq);
    cpb_assert_h(prev_len == new_len, "");
    
    return CPB_OK;
}


static int cpb_taskqueue_pop_next(struct cpb_taskqueue *tq, struct cpb_task *task_out) {
    if (cpb_taskqueue_len(tq) == 0)
        return CPB_OUT_OF_RANGE_ERR;
    struct cpb_task task = tq->tasks[tq->head];
    tq->head++;
    if (tq->head >= tq->cap) //tq->head %= tq->cap;
        tq->head = 0;
    *task_out = task;
    return CPB_OK;
}
static int cpb_taskqueue_init(struct cpb_taskqueue *tq, struct cpb* cpb_ref, int sz) {
    memset(tq, 0, sizeof *tq);
    tq->cpb = cpb_ref;
    if (sz != 0) {
        return cpb_taskqueue_resize(tq, sz);
    }
    return CPB_OK;
}
static int cpb_taskqueue_deinit(struct cpb_taskqueue *tq) {
    //TODO destroy pending events
    cpb_free(tq->cpb, tq->tasks);
    return CPB_OK;
}

#endif // CPB_THREADPOOL_H