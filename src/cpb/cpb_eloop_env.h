#ifndef CPB_ELOOP_ENV_H
#define CPB_ELOOP_ENV_H
#define CPB_MAX_ELOOPS 128
#include "cpb_eloop.h"
#include "cpb_thread.h"
#include "cpb_threadpool.h"

//unsafe to move in memory, members hold references to &tp
struct cpb_eloop_env {
    struct {
        struct cpb_eloop *loop;
        struct cpb_thread *thread;
    } loops[CPB_MAX_ELOOPS];
    int nloops;
    int rr;

    struct cpb_threadpool tp;
    struct cpb *cpb_ref;
};
static int cpb_eloop_env_init(struct cpb_eloop_env *elist, struct cpb *cpb_ref, int nloops) {
    int err = CPB_OK;
    if (nloops > CPB_MAX_ELOOPS)
        return CPB_INVALID_ARG_ERR;
    elist->rr = 0;
    elist->nloops = 0;
    elist->cpb_ref = cpb_ref;

    err = cpb_threadpool_init(&elist->tp, cpb_ref);
    if (err != CPB_OK) {
        goto err0;
    }

    for (int i=0; i<nloops; i++) {
        void *p = cpb_malloc(cpb_ref, sizeof(struct cpb_eloop));
        if (!p)
            goto err1;
        elist->loops[i].loop = p;
        err = cpb_eloop_init(elist->loops[i].loop, i, cpb_ref, &elist->tp, 256);
        if (err != CPB_OK) {
            cpb_free(cpb_ref, p);
            goto err1;
        }
        elist->loops[i].thread = NULL;
        elist->nloops++;
    }
    return CPB_OK;
err1:
    for (int i=0; i<elist->nloops; i++) {
        cpb_eloop_deinit(elist->loops[i].loop);
        cpb_free(cpb_ref, elist->loops[i].loop);
    }
err0:
    return err;
}
static struct cpb_eloop * cpb_eloop_env_get_any(struct cpb_eloop_env *elist) {
    struct cpb_eloop *eloop = elist->loops[elist->rr].loop;
    if (++elist->rr >= elist->nloops)
        elist->rr = 0; // %= elist->nloops
    return eloop;
}
static struct cpb_eloop * cpb_eloop_env_stop(struct cpb_eloop_env *elist) {
    for (int i=0; i<elist->nloops; i++) {
        cpb_eloop_stop(elist->loops[i].loop);
    }
}

static void *cpb_eloop_env_thread_runner(void *p) {
    struct cpb_thread *t = p;
    struct cpb_eloop *eloop = t->data;
    cpb_eloop_run(eloop);
    return NULL;
}

static struct cpb_error cpb_eloop_env_run(struct cpb_eloop_env *elist) {
    for (int i=0; i<elist->nloops; i++) {
        cpb_thread_new(elist->cpb_ref, i, &elist->tp, cpb_eloop_env_thread_runner, elist->loops[i].loop, &elist->loops[i].thread);
    }
    return cpb_make_error(CPB_OK);
}
static struct cpb_error  cpb_eloop_env_join(struct cpb_eloop_env *elist) {
    for (int i=0; i<elist->nloops; i++) {
        int rv = cpb_thread_join(elist->loops[i].thread);
        cpb_thread_destroy(elist->loops[i].thread, elist->cpb_ref);
        elist->loops[i].thread = NULL;
    }
    
    return cpb_make_error(CPB_OK);
}

static int cpb_eloop_env_deinit(struct cpb_eloop_env *elist) {
    for (int i=0; i<elist->nloops; i++) {
        cpb_eloop_deinit(elist->loops[i].loop);
        cpb_free(elist->cpb_ref, elist->loops[i].loop);
    }
    cpb_threadpool_deinit(&elist->tp);
}

#endif // CPB_ELOOP_ENV_H