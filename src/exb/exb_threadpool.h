#ifndef EXB_THREADPOOL_H
#define EXB_THREADPOOL_H
#include <string.h>
#include <stdio.h>

#include "exb.h"
#include "exb_errors.h"
#include "exb_utils.h"
#include "exb_task.h"
#include "exb_thread.h"

#define EXB_THREAD_TASK_BUFFER_COUNT 16
int exb_hw_cpu_count();
int exb_hw_bind_to_core(int core_id);
int exb_hw_bind_not_to_core(int core_id);
int exb_hw_thread_sched_important();
int exb_hw_thread_sched_background();
struct exb_taskqueue {
    struct exb *exb; //not owned, must outlive
    struct exb_task *tasks;
    int head;
    int tail; //tail == head means full, tail == head - 1 means full
    int cap;
};
struct exb_threadpool {
    int tid;

    pthread_mutex_t tp_mtx;
    pthread_cond_t tp_cnd;

    struct exb *exb; //not owned, must outlive
    struct exb_thread **threads; /*array of pointers*/
    int nthreads;

    struct exb_taskqueue taskq;
};

static int exb_taskqueue_deinit(struct exb_taskqueue *tq); //fwd
static int exb_taskqueue_init(struct exb_taskqueue *tq, struct exb* exb_ref, int sz);

static int exb_threadpool_init(struct exb_threadpool *tp, struct exb* exb_ref) {
    tp->tid = 0;
    tp->nthreads = 0;
    tp->threads = NULL;
    int err  = EXB_OK;
    if (pthread_mutex_init(&tp->tp_mtx, NULL) != 0 ||
        pthread_cond_init(&tp->tp_cnd, NULL) != 0) 
    {
        err = EXB_MUTEX_ERROR;
    }
    else {
        err = exb_taskqueue_init(&tp->taskq, exb_ref, 128);
    }
    return err;
}
static void exb_threadpool_deinit(struct exb_threadpool *tp) {
    for (int i=0; i<tp->nthreads; i++) {
        /*ASSUMES THREAD STOPPED EXECUTING*/
        exb_thread_destroy(tp->threads[i], tp->exb);
    }
    exb_free(tp->exb, tp->threads);
    exb_taskqueue_deinit(&tp->taskq);
}

static int exb_taskqueue_pop_next(struct exb_taskqueue *tq, struct exb_task *task_out);
static int exb_taskqueue_pop_many(struct exb_taskqueue *tq, struct exb_task *tasks_out, int *ntasks_out, int max_tasks);
static int exb_taskqueue_append(struct exb_taskqueue *tq, struct exb_task task);
static int exb_taskqueue_append_many(struct exb_taskqueue *tq, struct exb_task *tasks, int ntasks);
static int exb_taskqueue_len(struct exb_taskqueue *tq);

/*Must be threadsafe*/
/*TODO: optimize, use a CAS/LLSC algorithm with a fallback on mutexes*/
static int exb_threadpool_push_task(struct exb_threadpool *tp, struct exb_task task) {
    if (pthread_mutex_lock(&tp->tp_mtx) != 0) {
        return EXB_MUTEX_LOCK_ERROR;
    }
    int err = EXB_OK;
    err = exb_taskqueue_append(&tp->taskq, task);
//ret:
    pthread_cond_signal(&tp->tp_cnd);
    pthread_mutex_unlock(&tp->tp_mtx);
    return err;
}


static int exb_threadpool_push_tasks_many(struct exb_threadpool *tp, struct exb_task *tasks, int ntasks) {
    if (pthread_mutex_lock(&tp->tp_mtx) != 0) {
        return EXB_MUTEX_LOCK_ERROR;
    }
    int err = EXB_OK;
    err = exb_taskqueue_append_many(&tp->taskq, tasks, ntasks);

//ret:
    pthread_cond_signal(&tp->tp_cnd);
    pthread_mutex_unlock(&tp->tp_mtx);
    return err;
}


/*Must be threadsafe*/
/*TODO: optimize, use a CAS/LLSC algorithm with a fallback on mutexes*/
static int exb_threadpool_pop_task(struct exb_threadpool *tp, struct exb_task *task_out) {
    if (pthread_mutex_lock(&tp->tp_mtx) != 0) {
        return EXB_MUTEX_LOCK_ERROR;
    }
    int err = EXB_OK;
    err = exb_taskqueue_pop_next(&tp->taskq, task_out);
//ret:
    pthread_mutex_unlock(&tp->tp_mtx);
    return err;
}

/*Must be threadsafe*/
/*TODO: optimize, use a CAS/LLSC algorithm with a fallback on mutexes*/
static int exb_threadpool_pop_tasks_many(struct exb_threadpool *tp, struct exb_task *tasks_out, int *ntasks_out, int max_tasks) {
    if (pthread_mutex_lock(&tp->tp_mtx) != 0) {
        return EXB_MUTEX_LOCK_ERROR;
    }
    int err = EXB_OK;
    err = exb_taskqueue_pop_many(&tp->taskq, tasks_out, ntasks_out, max_tasks);
//ret:
    pthread_mutex_unlock(&tp->tp_mtx);
    return err;
}


/*Must be threadsafe*/
/*Waits until new tasks are available (this can be done way better)*/
static int exb_threadpool_wait_for_work_and_lock(struct exb_threadpool *tp) {
    int rv = pthread_mutex_lock(&tp->tp_mtx);
    if (rv != 0) {
        return EXB_MUTEX_LOCK_ERROR;
    }
    while (exb_taskqueue_len(&tp->taskq) == 0) {
        rv = pthread_cond_wait(&tp->tp_cnd, &tp->tp_mtx);
        if (rv != 0) {
            return EXB_MUTEX_LOCK_ERROR;
        }
    }

    return EXB_OK;
}

/*Must be threadsafe*/
/*Waits until new tasks are available (this can be done way better)*/
static int exb_threadpool_wait_for_work(struct exb_threadpool *tp) {
    int rv = exb_threadpool_wait_for_work_and_lock(tp);
    if (rv == EXB_OK) {
        pthread_mutex_unlock(&tp->tp_mtx);
    }
    return rv;
}


static void  *exb_threadpool_thread_run(void *arg) {
    #if 0
        exb_hw_bind_not_to_core(0);
        exb_hw_thread_sched_background();
    #endif
    struct exb_thread *t = arg;
    struct exb_task current_tasks[EXB_THREAD_TASK_BUFFER_COUNT];
    int ntasks = 0;
    while (1) {
        int rv = exb_threadpool_pop_tasks_many(t->tp, current_tasks, &ntasks, EXB_THREAD_TASK_BUFFER_COUNT);
        if (rv != EXB_OK) { 
            break;
        }
        else if (ntasks == 0) {
            exb_threadpool_wait_for_work(t->tp);
        }
        for (int i=0; i<ntasks; i++) {
            current_tasks[i].run(t, &current_tasks[i]);
        }
    }
    return arg;
}


static int exb_threadpool_set_nthreads(struct exb_threadpool *tp, int nthreads) {
    if (pthread_mutex_lock(&tp->tp_mtx) != 0) {
        return EXB_MUTEX_LOCK_ERROR;
    }
    int err = EXB_OK;
    int old_thread_count = tp->nthreads;

    if (nthreads == old_thread_count) {
        err = EXB_OK;
        goto ret;
    }
    else if (nthreads < old_thread_count) {
        err = EXB_UNSUPPORTED;
        goto ret;
    }
    exb_assert_s(nthreads > old_thread_count, "");

    {
        void *p = exb_realloc(tp->exb, tp->threads, sizeof(struct exb_thread) * nthreads);
        if (!p) {
            err = EXB_NOMEM_ERR;
            goto ret;
        }
        tp->threads = p;
    }
    
    for (int i=old_thread_count; i<nthreads; i++) {
        int rv = exb_thread_new(tp->exb, tp->tid, tp, exb_threadpool_thread_run, NULL, &tp->threads[i]);
        if (rv != EXB_OK) {
            for (int j=i-1; j>=old_thread_count; j--) {
                exb_thread_cancel_and_destroy(tp->threads[j], tp->exb);
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

static int exb_taskqueue_len(struct exb_taskqueue *tq) {
    if (tq->tail >= tq->head) {
        return tq->tail - tq->head;
    }
    return tq->cap - tq->head + tq->tail;
}
static int exb_taskqueue_resize(struct exb_taskqueue *tq, int sz);
static int exb_taskqueue_append(struct exb_taskqueue *tq, struct exb_task task) {
    if (exb_taskqueue_len(tq) == tq->cap - 1) {
        int nsz = tq->cap * 2;
        int rv = exb_taskqueue_resize(tq, nsz > 0 ? nsz : 4);
        if (rv != EXB_OK)
            return rv;
    }
    tq->tasks[tq->tail] = task;
    tq->tail++;
    if (tq->tail >= tq->cap) //tq->tail %= tq->cap;
        tq->tail = 0;
    return EXB_OK;
}

//see also exb_eloop_append_many()
static int exb_taskqueue_append_many(struct exb_taskqueue *tq, struct exb_task *tasks, int ntasks) {
    if ((exb_taskqueue_len(tq) + ntasks) >= tq->cap - 1) {
        int nsz = tq->cap ? tq->cap * 2 : 4;
        while ((exb_taskqueue_len(tq) + ntasks) >= nsz - 1)
            nsz *= 2;
        int rv = exb_taskqueue_resize(tq, nsz);
        if (rv != EXB_OK)
            return rv;
    }
    exb_assert_h((exb_taskqueue_len(tq) + ntasks) < tq->cap - 1, "");

    if (tq->head > tq->tail) {
        //example: cap=8
        //         []
        //indices: 0  1  2  3  4  5  6  7
        //               ^tail       ^head
        //               ^^^^^^^^^^
        //               at tail
        // no at head
        exb_assert_h(tq->tail + ntasks < tq->head, "");
        memcpy(tq->tasks +tq->tail, tasks, ntasks * sizeof(struct exb_task));
        tq->tail += ntasks;
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
        int at_tail = tq->cap - tq->tail;
        if (at_tail > ntasks) {
            at_tail = ntasks;
        }
        memcpy(tq->tasks + tq->tail, tasks, at_tail * sizeof(struct exb_task));
        int remainder = ntasks - at_tail;
        if (remainder > 0) {
            exb_assert_h(tq->head > remainder, "");
            //A STUPID BUG WAS FIXED HERE!
            memcpy(tq->tasks, tasks + at_tail, remainder * sizeof(struct exb_task));
            tq->tail = remainder;
        }
        else {
            exb_assert_h(remainder == 0, "");
            tq->tail += at_tail;
            if (tq->tail >= tq->cap)
                tq->tail = 0;
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


static int exb_taskqueue_resize(struct exb_taskqueue *tq, int sz) {
    exb_assert_h(!!tq->exb, "");
    void *p = exb_malloc(tq->exb, sizeof(struct exb_task) * sz);

    if (!p) {
        return EXB_NOMEM_ERR;
    }
    //this can be optimized, see also eloop
    struct exb_task *tasks = p;
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
    exb_free(tq->exb, tq->tasks);
    int prev_len = exb_taskqueue_len(tq);
    tq->tasks = p;
    tq->head = 0;
    tq->tail = idx;
    tq->cap = sz;
    int new_len = exb_taskqueue_len(tq);
    exb_assert_h(prev_len == new_len, "");
    
    return EXB_OK;
}


static int exb_taskqueue_pop_next(struct exb_taskqueue *tq, struct exb_task *task_out) {
    if (exb_taskqueue_len(tq) == 0)
        return EXB_OUT_OF_RANGE_ERR;
    struct exb_task task = tq->tasks[tq->head];
    tq->head++;
    if (tq->head >= tq->cap) //tq->head %= tq->cap;
        tq->head = 0;
    *task_out = task;
    return EXB_OK;
}


static int exb_taskqueue_pop_many(struct exb_taskqueue *tq, struct exb_task *tasks_out, int *ntasks_out, int max_tasks) {
    int navailable = exb_taskqueue_len(tq);
    if (navailable > max_tasks)
        navailable = max_tasks;
    for (int i=0; i<navailable; i++){
        exb_assert_h(tq->head != tq->tail, "");
        tasks_out[i] = tq->tasks[tq->head];
        tq->head++;
        if (tq->head >= tq->cap) //tq->head %= tq->cap;
            tq->head = 0;
    }
    *ntasks_out = navailable;
    return EXB_OK;
}

static int exb_taskqueue_init(struct exb_taskqueue *tq, struct exb* exb_ref, int sz) {
    memset(tq, 0, sizeof *tq);
    tq->exb = exb_ref;
    if (sz != 0) {
        return exb_taskqueue_resize(tq, sz);
    }
    return EXB_OK;
}
static int exb_taskqueue_deinit(struct exb_taskqueue *tq) {
    //TODO destroy pending events
    exb_free(tq->exb, tq->tasks);
    return EXB_OK;
}


#endif // EXB_THREADPOOL_H