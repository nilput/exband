#include <stdlib.h>
#ifndef CPB_H
#define CPB_H

#define CPB_RESTRICT 

#include "cpb_assert.h"
#include "cpb_errors.h"
#define RQSTATE_EVENT(f, fmt, ...) 

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

struct cpb {
    //this can be used for custom allocators in the future
    //and semi global state, (GOD OBJECT)
    int e;
};
static int cpb_init(struct cpb *cpb) {
    return 0;
}
static int cpb_deinit(struct cpb *cpb) {
    return 0;
}
static void *cpb_malloc(struct cpb *cpb, size_t sz){
    void *m = malloc(sz);
    return m;
}
static void *cpb_realloc(struct cpb *cpb, void *p, size_t sz){
    void *m = realloc(p, sz);
    return m;
}
static void cpb_free(struct cpb *cpb, void *p){
    free(p);
}
#endif
