#include <stdlib.h>
#ifndef CPB_H
#define CPB_H
#include "cpb_assert.h"
struct cpb {
    //this can be used for custom allocators in the future
    int e;
};
int cpb_init(struct cpb *cpb) {
    return 0;
}
int cpb_deinit(struct cpb *cpb) {
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