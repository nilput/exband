#ifndef EXB_EVLOOP_POOL_H
#define EXB_EVLOOP_POOL_H

#include "exb_build_config.h"
#include "exb_evloop.h"
#include "exb_thread.h"
#include "exb_threadpool.h"

//unsafe to move in memory, members hold references to &tp
struct exb_evloop_pool {
    struct {
        struct exb_evloop *loop;
        struct exb_thread *thread;
    } loops[EXB_MAX_EVLOOPS];
    int nloops;
    int rr; //round robin counter for distributing load

    struct exb_threadpool tp;
    struct exb *exb_ref;
};

static int exb_evloop_pool_init(struct exb_evloop_pool *elist, struct exb *exb_ref, int nloops) {
    int err = EXB_OK;
    if (nloops > EXB_MAX_EVLOOPS)
        return EXB_INVALID_ARG_ERR;
    elist->rr = 0;
    elist->nloops = 0;
    elist->exb_ref = exb_ref;

    err = exb_threadpool_init(&elist->tp, exb_ref);
    if (err != EXB_OK) {
        goto err0;
    }

    for (int i=0; i<nloops; i++) {
        void *p = exb_malloc(exb_ref, sizeof(struct exb_evloop));
        if (!p)
            goto err1;
        elist->loops[i].loop = p;
        err = exb_evloop_init(elist->loops[i].loop, i, exb_ref, &elist->tp, 256);
        if (err != EXB_OK) {
            exb_free(exb_ref, p);
            goto err1;
        }
        elist->loops[i].thread = NULL;
        elist->nloops++;
    }
    return EXB_OK;
err1:
    for (int i=0; i<elist->nloops; i++) {
        exb_evloop_deinit(elist->loops[i].loop);
        exb_free(exb_ref, elist->loops[i].loop);
    }
err0:
    return err;
}

static struct exb_evloop * exb_evloop_pool_get_any(struct exb_evloop_pool *elist) {
    struct exb_evloop *evloop = elist->loops[elist->rr].loop;
    if (++elist->rr >= elist->nloops)
        elist->rr = 0; // %= elist->nloops
    return evloop;
}

static int exb_evloop_pool_stop(struct exb_evloop_pool *elist) {
    for (int i=0; i<elist->nloops; i++) {
        exb_evloop_stop(elist->loops[i].loop);
    }
    return EXB_OK;
}

static void *exb_evloop_pool_thread_runner(void *p) {
    struct exb_thread *t = p;
    struct exb_evloop *evloop = t->data;
    #ifdef EXB_SCHED
        if (t->bind_cpu != -1) {
            
            int ncores = exb_hw_cpu_count();
            if (ncores > 0) { 
                t->bind_cpu %= ncores;
                exb_hw_bind_to_core(t->bind_cpu);
            }
            exb_hw_thread_sched_important();
        }
    #endif
    exb_evloop_run(evloop);
    return NULL;
}

//cpu offset is used for cpu affinity, it's not important (can be -1)
static struct exb_error exb_evloop_pool_run(struct exb_evloop_pool *elist, int cpu_offset) {
    for (int i=0; i<elist->nloops; i++) {
        exb_thread_new(elist->exb_ref, i, &elist->tp, exb_evloop_pool_thread_runner, elist->loops[i].loop, &elist->loops[i].thread);
        elist->loops[i].thread->bind_cpu = cpu_offset + i;
    }
    return exb_make_error(EXB_OK);
}

static struct exb_error  exb_evloop_pool_join(struct exb_evloop_pool *elist) {
    for (int i=0; i<elist->nloops; i++) {
        if (elist->loops[i].thread) {
            int rv = exb_thread_join(elist->loops[i].thread);
            exb_thread_destroy(elist->loops[i].thread, elist->exb_ref);
            elist->loops[i].thread = NULL;
        }
    }
    
    return exb_make_error(EXB_OK);
}

static int exb_evloop_pool_deinit(struct exb_evloop_pool *elist) {
    for (int i=0; i<elist->nloops; i++) {
        exb_evloop_deinit(elist->loops[i].loop);
        exb_free(elist->exb_ref, elist->loops[i].loop);
    }
    exb_threadpool_deinit(&elist->tp);
    return EXB_OK;
}

#endif // EXB_EVLOOP_POOL_H
