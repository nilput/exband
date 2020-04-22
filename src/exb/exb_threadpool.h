#ifndef CPB_THREADPOOL_H
#define CPB_THREADPOOL_H
#include <string.h>
#include <stdio.h>

#include "cpb.h"
#include "cpb_errors.h"
#include "cpb_utils.h"
#include "cpb_task.h"
#include "cpb_thread.h"

#define CPB_THREAD_TASK_BUFFER_COUNT 16
int cpb_hw_cpu_count();
int cpb_hw_bind_to_core(int core_id);
int cpb_hw_bind_not_to_core(int core_id);
int cpb_hw_thread_sched_important();
int cpb_hw_thread_sched_background();
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
    pthread_cond_t tp_cnd;

    struct cpb *cpb; //not owned, must outlive
    struct cpb_thread **threads; /*array of pointers*/
    int nthreads;

    struct cpb_taskqueue taskq;
};

static int cpb_taskqueue_deinit(struct cpb_taskqueue *tq); //fwd
static int cpb_taskqueue_init(struct cpb_taskqueue *tq, struct cpb* cpb_ref, int sz);

static int cpb_threadpool_init(struct cpb_threadpool *tp, struct cpb* cpb_ref) {
    tp->tid = 0;
    tp->nthreads = 0;
    tp->threads = NULL;
    int err  = CPB_OK;
    if (pthread_mutex_init(&tp->tp_mtx, NULL) != 0 ||
        pthread_cond_init(&tp->tp_cnd, NULL) != 0) 
    {
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
        cpb_thread_destroy(tp->threads[i], tp->cpb);
    }
    cpb_free(tp->cpb, tp->threads);
    cpb_taskqueue_deinit(&tp->taskq);
}

static int cpb_taskqueue_pop_next(struct cpb_taskqueue *tq, struct cpb_task *task_out);
static int cpb_taskqueue_pop_many(struct cpb_taskqueue *tq, struct cpb_task *tasks_out, int *ntasks_out, int max_tasks);
static int cpb_taskqueue_append(struct cpb_taskqueue *tq, struct cpb_task task);
static int cpb_taskqueue_append_many(struct cpb_taskqueue *tq, struct cpb_task *tasks, int ntasks);
static int cpb_taskqueue_len(struct cpb_taskqueue *tq);

/*Must be threadsafe*/
/*TODO: optimize, use a CAS/LLSC algorithm with a fallback on mutexes*/
static int cpb_threadpool_push_task(struct cpb_threadpool *tp, struct cpb_task task) {
    if (pthread_mutex_lock(&tp->tp_mtx) != 0) {
        return CPB_MUTEX_LOCK_ERROR;
    }
    int err = CPB_OK;
    err = cpb_taskqueue_append(&tp->taskq, task);
//ret:
    pthread_cond_signal(&tp->tp_cnd);
    pthread_mutex_unlock(&tp->tp_mtx);
    return err;
}


static int cpb_threadpool_push_tasks_many(struct cpb_threadpool *tp, struct cpb_task *tasks, int ntasks) {
    if (pthread_mutex_lock(&tp->tp_mtx) != 0) {
        return CPB_MUTEX_LOCK_ERROR;
    }
    int err = CPB_OK;
    err = cpb_taskqueue_append_many(&tp->taskq, tasks, ntasks);

//ret:
    pthread_cond_signal(&tp->tp_cnd);
    pthread_mutex_unlock(&tp->tp_mtx);
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
//ret:
    pthread_mutex_unlock(&tp->tp_mtx);
    return err;
}

/*Must be threadsafe*/
/*TODO: optimize, use a CAS/LLSC algorithm with a fallback on mutexes*/
static int cpb_threadpool_pop_tasks_many(struct cpb_threadpool *tp, struct cpb_task *tasks_out, int *ntasks_out, int max_tasks) {
    if (pthread_mutex_lock(&tp->tp_mtx) != 0) {
        return CPB_MUTEX_LOCK_ERROR;
    }
    int err = CPB_OK;
    err = cpb_taskqueue_pop_many(&tp->taskq, tasks_out, ntasks_out, max_tasks);
//ret:
    pthread_mutex_unlock(&tp->tp_mtx);
    return err;
}


/*Must be threadsafe*/
/*Waits until new tasks are available (this can be done way better)*/
static int cpb_threadpool_wait_for_work_and_lock(struct cpb_threadpool *tp) {
    int rv = pthread_mutex_lock(&tp->tp_mtx);
    if (rv != 0) {
        return CPB_MUTEX_LOCK_ERROR;
    }
    while (cpb_taskqueue_len(&tp->taskq) == 0) {
        rv = pthread_cond_wait(&tp->tp_cnd, &tp->tp_mtx);
        if (rv != 0) {
            return CPB_MUTEX_LOCK_ERROR;
        }
    }

    return CPB_OK;
}

/*Must be threadsafe*/
/*Waits until new tasks are available (this can be done way better)*/
static int cpb_threadpool_wait_for_work(struct cpb_threadpool *tp) {
    int rv = cpb_threadpool_wait_for_work_and_lock(tp);
    if (rv == CPB_OK) {
        pthread_mutex_unlock(&tp->tp_mtx);
    }
    return rv;
}


static void  *cpb_threadpool_thread_run(void *arg) {
    #if 0
        cpb_hw_bind_not_to_core(0);
        cpb_hw_thread_sched_background();
    #endif
    struct cpb_thread *t = arg;
    struct cpb_task current_tasks[CPB_THREAD_TASK_BUFFER_COUNT];
    int ntasks = 0;
    while (1) {
        int rv = cpb_threadpool_pop_tasks_many(t->tp, current_tasks, &ntasks, CPB_THREAD_TASK_BUFFER_COUNT);
        if (rv != CPB_OK) { 
            break;
        }
        else if (ntasks == 0) {
            cpb_threadpool_wait_for_work(t->tp);
            
        }
        for (int i=0; i<ntasks; i++) {
            current_tasks[i].run(t, &current_tasks[i]);
        }
    }
    return arg;
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
        int rv = cpb_thread_new(tp->cpb, tp->tid, tp, cpb_threadpool_thread_run, NULL, &tp->threads[i]);
        if (rv != CPB_OK) {
            for (int j=i-1; j>=old_thread_count; j--) {
                cpb_thread_cancel_and_destroy(tp->threads[j], tp->cpb);
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

//see also cpb_eloop_append_many()
static int cpb_taskqueue_append_many(struct cpb_taskqueue *tq, struct cpb_task *tasks, int ntasks) {
    if ((cpb_taskqueue_len(tq) + ntasks) >= tq->cap - 1) {
        int nsz = tq->cap ? tq->cap * 2 : 4;
        while ((cpb_taskqueue_len(tq) + ntasks) >= nsz - 1)
            nsz *= 2;
        int rv = cpb_taskqueue_resize(tq, nsz);
        if (rv != CPB_OK)
            return rv;
    }
    cpb_assert_h((cpb_taskqueue_len(tq) + ntasks) < tq->cap - 1, "");

    if (tq->head > tq->tail) {
        //example: cap=8
        //         []
        //indices: 0  1  2  3  4  5  6  7
        //               ^tail       ^head
        //               ^^^^^^^^^^
        //               at tail
        // no at head
        cpb_assert_h(tq->tail + ntasks < tq->head, "");
        memcpy(tq->tasks +tq->tail, tasks, ntasks * sizeof(struct cpb_task));
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
        memcpy(tq->tasks + tq->tail, tasks, at_tail * sizeof(struct cpb_task));
        int remainder = ntasks - at_tail;
        if (remainder > 0) {
            cpb_assert_h(tq->head > remainder, "");
            //A STUPID BUG WAS FIXED HERE!
            memcpy(tq->tasks, tasks + at_tail, remainder * sizeof(struct cpb_task));
            tq->tail = remainder;
        }
        else {
            cpb_assert_h(remainder == 0, "");
            tq->tail += at_tail;
            if (tq->tail >= tq->cap)
                tq->tail = 0;
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


static int cpb_taskqueue_pop_many(struct cpb_taskqueue *tq, struct cpb_task *tasks_out, int *ntasks_out, int max_tasks) {
    int navailable = cpb_taskqueue_len(tq);
    if (navailable > max_tasks)
        navailable = max_tasks;
    for (int i=0; i<navailable; i++){
        cpb_assert_h(tq->head != tq->tail, "");
        tasks_out[i] = tq->tasks[tq->head];
        tq->head++;
        if (tq->head >= tq->cap) //tq->head %= tq->cap;
            tq->head = 0;
    }
    *ntasks_out = navailable;
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