#ifndef CPB_REQUEST_STATE_RECYCLE_ARRAY_H
#define CPB_REQUEST_STATE_RECYCLE_ARRAY_H
#include "../cpb.h"
struct cpb_request_state_recycle_array {
    struct cpb_request_state **elements;
    int cap;
    int len;
};

static int cpb_request_state_recycle_array_init(struct cpb *cpb_ref, struct cpb_request_state_recycle_array *rq_cyc) {
    rq_cyc->elements = NULL;
    rq_cyc->cap = 0;
    rq_cyc->len = 0;
    return CPB_OK;
}
static int cpb_request_state_recycle_array_resize(struct cpb *cpb_ref, struct cpb_request_state_recycle_array *rq_cyc, int size) {
    for (; rq_cyc->len > size; rq_cyc->len--) {
        cpb_free(cpb_ref, rq_cyc->elements[rq_cyc->len - 1]);
    }
    void *p = cpb_realloc(cpb_ref, rq_cyc->elements, size * sizeof(struct cpb_request_state *));
    if (!p && size > 0) {
        return CPB_NOMEM_ERR;
    }
    rq_cyc->elements = p;
    rq_cyc->cap = size;
    return CPB_OK;
}
static int cpb_request_state_recycle_array_push(struct cpb *cpb_ref, struct cpb_request_state_recycle_array *rq_cyc, struct cpb_request_state *rqstate) {
    if (rq_cyc->len + 1 > rq_cyc->cap) {
       int new_cap = rq_cyc->cap == 0 ? 16 : rq_cyc->cap * 2;
       int rv;
       if ((rv = cpb_request_state_recycle_array_resize(cpb_ref, rq_cyc, new_cap)) != CPB_OK)
            return rv;
    }
    rq_cyc->elements[rq_cyc->len++] = rqstate;
    
    return CPB_OK;
}
static int cpb_request_state_recycle_array_pop(struct cpb *cpb_ref, struct cpb_request_state_recycle_array *rq_cyc, struct cpb_request_state **rqstate_out) {
    (void) cpb_ref;
    if (rq_cyc->len < 1)
        return CPB_OUT_OF_RANGE_ERR;
    *rqstate_out = rq_cyc->elements[--rq_cyc->len];
    return CPB_OK;
}
static void cpb_request_state_recycle_array_deinit(struct cpb *cpb_ref, struct cpb_request_state_recycle_array *rq_cyc) {
    for (int i=0; i<rq_cyc->len; i++) {
        cpb_free(cpb_ref, rq_cyc->elements[i]);
    }
    cpb_free(cpb_ref, rq_cyc->elements);
    rq_cyc->elements = NULL;
    rq_cyc->cap = 0;
    rq_cyc->len = 0;
}

#endif
