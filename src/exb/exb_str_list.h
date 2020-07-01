#ifndef EXB_STR_LIST_H
#define EXB_STR_LIST_H
#include "exb_str.h"
#include "exb.h"
#include <string.h>

struct exb_str_list {
    struct exb_str *elements;
    size_t len;
    size_t cap;
};

static int exb_str_list_init(struct exb *exb_ref, struct exb_str_list *strlist) {
    strlist->elements = NULL;
    strlist->len = 0;
    strlist->cap = 0;
    return EXB_OK;
}


static int exb_str_list_clear(struct exb *exb_ref, struct exb_str_list *strlist) {
    for (int i = 0; i < strlist->len; i++) {
        exb_str_deinit(exb_ref, strlist->elements + i);
    }
    strlist->len = 0;
    return EXB_OK;
}

static int exb_str_list_deinit(struct exb *exb_ref, struct exb_str_list *strlist) {
    exb_str_list_clear(exb_ref, strlist);
    exb_free(exb_ref, strlist->elements);
    strlist->elements = NULL;
    strlist->cap = 0;
    return EXB_OK;
}


//String is owned by the list
static int exb_str_list_push(struct exb *exb_ref, struct exb_str_list *strlist, struct exb_str *str) {
    if (strlist->len == strlist->cap) {
        if (!strlist->cap)
            strlist->cap = 1;
        strlist->cap *= 2;
        void *p = exb_realloc(exb_ref, strlist->elements, sizeof(struct exb_str) * strlist->cap);
        if (!p) {
            return EXB_NOMEM_ERR;
        }
        strlist->elements = p;
    }
    memcpy(strlist->elements + strlist->len, str, sizeof *str);
    strlist->len++;
    return EXB_OK;
}

#endif //EXB_STR_LIST_H
