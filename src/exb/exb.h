#include <stdlib.h>
#ifndef EXB_H
#define EXB_H

#define EXB_RESTRICT 

#include "exb_assert.h"
#include "exb_errors.h"
#ifdef TRACK_RQSTATE_EVENTS
    #define RQSTATE_EVENT(f, fmt, ...) fprintf(f, "[%.6f]" fmt, exb_time(), __VA_ARGS__)
#else
    #define RQSTATE_EVENT(f, fmt, ...) 
#endif

#if defined(ENABLE_DBGPERF)
    #include "dbgperf/dbgperf.h"
#else
    #define dp_clear()
    #define dp_register_event(x)
    #define dp_end_event(x)
    #define dp_timed_log(x, ...)
    #define dp_timed_logv(x, ap)
    #define dp_dump()
    #define dp_useless(n)
#endif

#if 1
    #define EXB_ALIGN(bytes)  __attribute__((aligned(bytes)))
#else
    #define EXB_ALIGN(bytes)
#endif
#if 1
    #define USE_GNU_MEMMEM
#endif
#if 1
    #define EXB_SET_TCPNODELAY
#endif
#if 1
    #define EXB_SET_TCPQUICKACK
#endif
#if 1
    #define EXB_SCHED
#endif
struct exb {
    //this can be used for custom allocators in the future
    //and semi global state, (GOD OBJECT)
    int e;
};
static int exb_init(struct exb *exb) {
    return 0;
}
static int exb_deinit(struct exb *exb) {
    return 0;
}
static void *exb_malloc(struct exb *exb, size_t sz){
    void *m = malloc(sz);
    return m;
}
static void *exb_realloc(struct exb *exb, void *p, size_t sz){
    void *m = realloc(p, sz);
    return m;
}
static void exb_free(struct exb *exb, void *p){
    free(p);
}
#endif
