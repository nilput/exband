#ifndef EXB_REQUEST_STATE_RECYCLE_ARRAY_H
#define EXB_REQUEST_STATE_RECYCLE_ARRAY_H
#include "../exb.h"
struct exb_request_state_recycle_array {
    struct exb_request_state **elements;
    int cap;
    int len;
};

static int exb_request_state_recycle_array_init(struct exb *exb_ref, struct exb_request_state_recycle_array *rq_cyc) {
    rq_cyc->elements = NULL;
    rq_cyc->cap = 0;
    rq_cyc->len = 0;
    return EXB_OK;
}
static int exb_request_state_recycle_array_resize(struct exb *exb_ref, struct exb_request_state_recycle_array *rq_cyc, int size) {
    for (; rq_cyc->len > size; rq_cyc->len--) {
        exb_free(exb_ref, rq_cyc->elements[rq_cyc->len - 1]);
    }
    void *p = exb_realloc(exb_ref, rq_cyc->elements, size * sizeof(struct exb_request_state *));
    if (!p && size > 0) {
        return EXB_NOMEM_ERR;
    }
    rq_cyc->elements = p;
    rq_cyc->cap = size;
    return EXB_OK;
}
static int exb_request_state_recycle_array_push(struct exb *exb_ref, struct exb_request_state_recycle_array *rq_cyc, struct exb_request_state *rqstate) {
    if (rq_cyc->len + 1 > rq_cyc->cap) {
       int new_cap = rq_cyc->cap == 0 ? 16 : rq_cyc->cap * 2;
       int rv;
       if ((rv = exb_request_state_recycle_array_resize(exb_ref, rq_cyc, new_cap)) != EXB_OK)
            return rv;
    }
    rq_cyc->elements[rq_cyc->len++] = rqstate;
    
    return EXB_OK;
}
static int exb_request_state_recycle_array_pop(struct exb *exb_ref, struct exb_request_state_recycle_array *rq_cyc, struct exb_request_state **rqstate_out) {
    (void) exb_ref;
    if (rq_cyc->len < 1)
        return EXB_OUT_OF_RANGE_ERR;
    *rqstate_out = rq_cyc->elements[--rq_cyc->len];
    return EXB_OK;
}
static void exb_request_state_recycle_array_deinit(struct exb *exb_ref, struct exb_request_state_recycle_array *rq_cyc) {
    for (int i=0; i<rq_cyc->len; i++) {
        exb_free(exb_ref, rq_cyc->elements[i]);
    }
    exb_free(exb_ref, rq_cyc->elements);
    rq_cyc->elements = NULL;
    rq_cyc->cap = 0;
    rq_cyc->len = 0;
}

#endif
