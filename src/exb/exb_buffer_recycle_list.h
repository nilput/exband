#ifndef exb_buffer_recycle_list_H
#define exb_buffer_recycle_list_H
#include "exb.h"

#define EXB_BUFFER_LIST_NBITS 64

struct exb_buffer {
    char *buff;
    size_t size;
};
struct exb_buffer_list {
    struct exb_buffer *buffers;
    int len;
    int cap;
};
struct exb_buffer_recycle_list {
    struct exb_buffer_list blist[EXB_BUFFER_LIST_NBITS];
};

static int exb_buffer_recycle_list_init(struct exb *exb_ref, struct exb_buffer_recycle_list *buff_cyc) {
    for (int i=0; i<EXB_BUFFER_LIST_NBITS; i++) {
        buff_cyc->blist[i].buffers = NULL;
        buff_cyc->blist[i].len = 0;
        buff_cyc->blist[i].cap = 0;
    }
    return EXB_OK;
}
static int exb_buffer_list_resize(struct exb *exb_ref, struct exb_buffer_list *buff_list, int size) {
    for (; buff_list->len > size; buff_list->len--) {
        exb_free(exb_ref, buff_list->buffers[buff_list->len - 1].buff);
    }
    void *p = exb_realloc(exb_ref, buff_list->buffers, size * sizeof(struct exb_buffer));
    if (!p && size > 0) {
        return EXB_NOMEM_ERR;
    }
    buff_list->buffers = p;
    buff_list->cap = size;
    return EXB_OK;
}
//can be made faster
static int buffer_size_bits(size_t buffer_size) {
#ifdef __GNUC__
    return __builtin_clzll(1) - __builtin_clzll(buffer_size) + 1;
#else
    int nbits = 0;
    while (buffer_size > 0) {
        buffer_size /= 2;
        nbits++;
    }
    return nbits;
#endif
}

static int exb_buffer_recycle_list_push(struct exb *exb_ref, struct exb_buffer_recycle_list *buff_cyc, void *buff, size_t buff_sz) {
    int index = buffer_size_bits(buff_sz);
    exb_assert_h(index > 0 && index < EXB_BUFFER_LIST_NBITS, "");
    struct exb_buffer_list *blist = buff_cyc->blist + index;
    if (blist->len + 1 > blist->cap) {
       int new_cap = blist->cap == 0 ? 16 : blist->cap * 2;
       int rv;
       if ((rv = exb_buffer_list_resize(exb_ref, blist, new_cap)) != EXB_OK)
            return rv;
    }
    blist->buffers[blist->len].buff = buff;
    blist->buffers[blist->len].size = buff_sz;
    blist->len++;
    
    return EXB_OK;
}
static int exb_buffer_recycle_list_pop(struct exb *exb_ref, struct exb_buffer_recycle_list *buff_cyc,  size_t needed_size, void **buffer_out, size_t *buffer_size_out) {
    (void) exb_ref;
    int index = buffer_size_bits(needed_size);
    exb_assert_h(index < EXB_BUFFER_LIST_NBITS, "");
    struct exb_buffer_list *blist = buff_cyc->blist + index;
    for (int i = blist->len - 1; i >= 0; i--) {
        if (blist->buffers[i].size >= needed_size) {
            *buffer_size_out = blist->buffers[i].size;
            *buffer_out      = blist->buffers[i].buff;
            if (i != blist->len - 1) {
                blist->buffers[i] = blist->buffers[blist->len - 1];
            }
            blist->len--;
            return EXB_OK;
        }
    }
    return EXB_NOT_FOUND;
}
static int exb_buffer_recycle_list_pop_eager(struct exb *exb_ref, struct exb_buffer_recycle_list *buff_cyc,  size_t needed_size, void **buffer_out, size_t *buffer_size_out) {
    int index = buffer_size_bits(needed_size);
    while (index < EXB_BUFFER_LIST_NBITS - 1) {
        if (exb_buffer_recycle_list_pop(exb_ref, buff_cyc,
                                        needed_size, buffer_out,
                                        buffer_size_out         ) == EXB_OK)
        {
            return EXB_OK;
        }
        needed_size *= 2;
        index = buffer_size_bits(needed_size);
    }
    return EXB_NOT_FOUND;
}
static void exb_buffer_recycle_list_deinit(struct exb *exb_ref, struct exb_buffer_recycle_list *buff_cyc) {
    for (int i=0; i<EXB_BUFFER_LIST_NBITS; i++) {
        for (int j=0; j<buff_cyc->blist[i].len; j++) {
            exb_free(exb_ref, buff_cyc->blist[i].buffers[j].buff);
        }
        exb_free(exb_ref, buff_cyc->blist[i].buffers);
        buff_cyc->blist[i].buffers = NULL;
    }
}

#endif
