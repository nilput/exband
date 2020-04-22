#ifndef EXB_STR_H
#define EXB_STR_H
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "exb.h"
#include "exb_errors.h"
#define EXB_CRV(code)

/*
* valid states:
*   .len > 0 :   memory is owned by the object
*   .len == -1 : memory is read only, and not owned by the object
*   in both cases .len and .str are valid, .str points to the empty string if the string is just initialized
*/
struct exb_str {
    char *str; //null terminated
    int len;
    int cap; //negative values mean memory is not owned by us (const char * passed to us)
};
struct exb_str_slice {
    int index;
    int len;
};
//returned str is not null terminated!
static struct exb_str exb_str_slice_to_const_str(struct exb_str_slice slice, const char *base) {
    struct exb_str s;
    s.str = (char *)base + slice.index;
    s.len = slice.len;
    s.cap = -1;
    return s;
}
static int exb_str_is_const(struct exb_str *str) {
    return str->cap == -1;
}

static int exb_str_init_strlcpy(struct exb *exb, struct exb_str *str, const char *src, int src_len);
static int exb_str_strlcpy(struct exb *exb, struct exb_str *str, const char *src, int src_len);

//assumes str is already initialized
static int exb_str_slice_to_copied_str(struct exb *exb, struct exb_str_slice slice, const char *base, struct exb_str *out) {
    int rv = exb_str_strlcpy(exb, out, base + slice.index, slice.len);
    if (rv != EXB_OK)
        return rv;
    return EXB_OK;
}


static int exb_str_clone(struct exb *exb, struct exb_str *str) {
    return exb_str_init_strlcpy(exb, str, str->str, str->len);
}

static int exb_str_rtrim(struct exb *exb, struct exb_str *str) {
    int rv;
    if (exb_str_is_const(str) && ((rv = exb_str_clone(exb, str) != EXB_OK))) {
        return rv;
    }
    while (str->len > 0 && (str->str[str->len-1] == ' ' ||
                            str->str[str->len-1] == '\t'  ))
        str->len--;
    str->str[str->len] = 0;
    return EXB_OK;
}
static void exb_str_slice_trim(const char *base, struct exb_str_slice *slice) {
    while (slice->len > 0 && (base[slice->index] == ' ' ||
                              base[slice->index] == '\t'  ))
    {
        slice->index++;
        slice->len--;
    }        
    while (slice->len > 0 && (base[slice->index + slice->len - 1] == ' ' ||
                              base[slice->index + slice->len - 1] == '\t'  ))
        slice->len--;
}

static struct exb_str exb_str_const_view(struct exb_str *str) {
    struct exb_str view = *str;
    view.cap = -1;
    return view;
}


//see str valid states at kdata.h
static int exb_str_init(struct exb *exb, struct exb_str *str) {
    (void) exb;
    str->str = "";
    str->cap = -1;
    str->len = 0;
    return EXB_OK;
}

static int exb_str_init_empty(struct exb_str *str) {
    str->str = "";
    str->cap = -1;
    str->len = 0;
    return EXB_OK;
}

//src0 must outlive the object (doesn't own it)
static int exb_str_init_const_str(struct exb_str *str, const char *src0) {
    exb_assert_h(!!src0, "passed NULL string");
    str->str = (char *) src0;
    str->len = strlen(src0);
    str->cap = -1;
    return EXB_OK;
}

static int exb_str_deinit(struct exb *exb, struct exb_str *str) {
    if (str->cap >= 0) {
        exb_free(exb, str->str);
    }
    str->str = NULL;
    str->cap = 0;
    str->len = 0;
    return EXB_OK;
}

static int exb_str_strlcpy(struct exb *exb, struct exb_str *str, const char *src, int srclen);

static int exb_str_init_strlcpy(struct exb *exb, struct exb_str *str, const char *src, int src_len) {
    int rv = exb_str_init(exb, str);
    if (rv != EXB_OK)
        return rv;
    rv = exb_str_strlcpy(exb, str, src, src_len);
    if (rv != EXB_OK) {
        exb_str_deinit(exb, str);
        return rv;
    }
    return EXB_OK;
}

static int exb_str_new(struct exb *exb, struct exb_str **strp) {
    void *p = NULL;
    p = exb_malloc(exb, sizeof(struct exb_str));
    if (!p) {
        *strp = NULL;
        return EXB_NOMEM_ERR;
    }
    int rv = exb_str_init(exb, p);
    *strp = p;
    return rv;
}
static int exb_str_destroy(struct exb *exb, struct exb_str *strp) {
    int rv = exb_str_deinit(exb, strp);
    exb_free(exb, strp);

    return rv;
}
static int exb_str_strcpy(struct exb *exb, struct exb_str *str, const char *src0);
static int exb_str_new_strcpy(struct exb *exb, struct exb_str **strp, const char *src0) {
    int rv = exb_str_new(exb, strp); EXB_CRV(rv);
    rv = exb_str_strcpy(exb, *strp, src0);
    if (rv != EXB_OK) {
        exb_str_destroy(exb, *strp);
        *strp = NULL;
        return rv;
    }
    return EXB_OK;
}


static int exb_str_clear(struct exb *exb, struct exb_str *str) {
    (void) exb;
    if (str->cap > 0) {
        str->str[0] = 0;
        str->len = 0;
    }
    else {
        str->str = "";
        str->len = 0;
        str->cap = -1;
    }
    return EXB_OK;
}
static int exb_str_set_cap(struct exb *exb, struct exb_str *str, int capacity) {
    if (capacity == 0) {
        return exb_str_clear(exb, str);
    }
    if (capacity < (str->len + 1))
        str->len = capacity - 1;
    if (str->cap < 0) {
        void *p;
        p  = exb_malloc(exb, capacity);
        if (!p) {
            return EXB_NOMEM_ERR;
        }
        memcpy(p, str->str, str->len);
        str->str = p;
    }
    else {
        void *p = NULL;
        p = exb_realloc(exb, str->str, capacity);
        if (!p) {
            return EXB_NOMEM_ERR;
        }
        str->str = p;
    }
    str->str[str->len] = 0;
    str->cap = capacity;
    return EXB_OK;
}
//make sure capacity is at least the provided arg
static int exb_str_ensure_cap(struct exb *exb, struct exb_str *str, int capacity) {
    if (str->cap >= capacity) {
        return EXB_OK;
    }
    if (capacity < 16)
        capacity = 16;
    return exb_str_set_cap(exb, str, (capacity * 3) / 2);
}
static int exb_str_strlcpy(struct exb *exb, struct exb_str *str, const char *src, int srclen) {
    int rv;
    if (str->cap <= srclen) {
        rv = exb_str_set_cap(exb, str, srclen + 1);
        if (rv != EXB_OK) {
            return rv;
        }
    }
    memcpy(str->str, src, srclen);
    str->len = srclen;
    str->str[srclen] = 0;
    return EXB_OK;
}
static int exb_str_strlappend(struct exb *exb, struct exb_str *str, const char *src, int srclen) {
    int rv;
    if (str->cap <= (str->len + srclen)) {
        rv = exb_str_set_cap(exb, str, str->len + srclen + 1);
        if (rv != EXB_OK) {
            return rv;
        }
    }
    memcpy(str->str + str->len, src, srclen);
    str->len += srclen;
    str->str[str->len] = 0;
    return EXB_OK;
}
static int exb_str_charappend(struct exb *exb, struct exb_str *str, unsigned char ch) {
    char buff[1] = {ch};
    return exb_str_strlappend(exb, str, buff, 1);
}
static int exb_str_strcpy(struct exb *exb, struct exb_str *str, const char *src0) {
    return exb_str_strlcpy(exb, str, src0, strlen(src0));
}
static int exb_str_strappend(struct exb *exb, struct exb_str *str, const char *src0) {
    return exb_str_strlappend(exb, str, src0, strlen(src0));
}
static int exb_str_init_copy(struct exb *exb, struct exb_str *str, struct exb_str *src) {
    int rv = exb_str_init(exb, str); EXB_CRV(rv);
    rv = exb_str_strlcpy(exb, str, src->str, src->len);
    if (rv != EXB_OK) {
        exb_str_deinit(exb, str);
        return rv;
    }
    return EXB_OK;
}
static int exb_str_new_copy(struct exb *exb, struct exb_str **strp, struct exb_str *src) {
    int rv = exb_str_new(exb, strp);
    if (rv != EXB_OK) {
        return rv;
    }
    rv = exb_str_strlcpy(exb, *strp, src->str, src->len);
    if (rv != EXB_OK) {
        exb_str_destroy(exb, *strp);
        *strp = NULL;
    }
    return EXB_OK;
}
//exclusive end, inclusive begin
static int exb_str_mutsubstr(struct exb *exb, struct exb_str *str, int begin, int end) {
    exb_assert_h((begin <= end) && (begin <= str->len) && (end <= str->len), "invalid arguments to mutsubstr()");
    void *p = NULL;
    int len = end - begin;
    p = exb_malloc(exb, len + 1);
    if (!p)
        return EXB_NOMEM_ERR;
    memcpy(p, str->str + begin, len);
    exb_free(exb, str->str);
    str->str = p;
    str->len = len;
    str->str[len] = 0;
    return EXB_OK;
}
enum exb_str_mutstrip {
    EXB_STRIP_DEFAULT = 0,
    EXB_STRIP_LEFT = 1,
    EXB_STRIP_RIGHT = 2,
};
//stripchars are treated like a set, each assumed to be one char to be excluded repeatedly from beginning and end
static int exb_str_mutstrip(struct exb *exb, struct exb_str *str, const char *stripchars, enum exb_str_mutstrip opts) {
    if (!opts)
        opts = EXB_STRIP_LEFT + EXB_STRIP_RIGHT;
    int begin = 0;
    int end = str->len;
    if (opts & EXB_STRIP_LEFT) {
        for (int i=0; i < str->len && strchr(stripchars, str->str[i]); i++)
            begin++;
    }
    if (opts & EXB_STRIP_RIGHT) {
        for (int i=str->len - 1; i > begin && strchr(stripchars, str->str[i]); i--)
            end--;
    }
    if (begin == 0 && end == str->len)
        return EXB_OK;
    return exb_str_mutsubstr(exb, str, begin, end);
}

static int exb_strl_eq(const char *a, size_t alen, const char *b, size_t blen) {
    return alen == blen && (memcmp(a, b, alen) == 0);
}

//doesnt work on binary strings
static int exb_strcasel_eq(const char *a, size_t alen, const char *b, size_t blen) {
    return alen == blen && (strncasecmp(a, b, alen) == 0);
}
//boolean
static int exb_str_streqc(struct exb *exb, struct exb_str *str, const char *src0) {
    if (!str->len && !src0[0])
        return 1;
    if (!str->len || !src0[0])
        return 0;
    exb_assert_h(!!str->str, "invalid string passed to be compared");
    return strcmp(str->str, src0) == 0;
}
static int exb_str_startswithc(struct exb *exb, struct exb_str *str, const char *src0) {
    exb_assert_h(str->str && src0, "invalid string passed to be compared");
    return strncmp(str->str, src0, strlen(src0)) == 0;
}

//boolean
static int exb_str_streq(struct exb *exb, struct exb_str *str_a, struct exb_str *str_b) {
    if (str_a->len != str_b->len)
        return 0;
    else if (str_a->len == 0)
        return 1;
    return exb_strl_eq(str_a->str, str_a->len, str_b->str, str_a->len);
}


static int exb_str_init_strcpy(struct exb *exb, struct exb_str *str, const char *src0) {
    int rv = exb_str_init(exb, str);
    if (rv != EXB_OK)
        return rv;
    rv = exb_str_strcpy(exb, str, src0);
    if (rv != EXB_OK) {
        exb_str_deinit(exb, str);
        return rv;
    }
    return EXB_OK;
}


static int exb_vsprintf(struct exb *exb, struct exb_str *str, const char *fmt, va_list ap_in) {
    int rv;
    va_list ap;
    if (str->cap < 2) {
        rv = exb_str_set_cap(exb, str, 2);
        if (rv != EXB_OK) {
            return rv;
        }
    }
    va_copy(ap, ap_in);
    int needed = vsnprintf(str->str, str->cap, fmt, ap);
    va_end(ap);
    if (needed >= str->cap) {
        rv = exb_str_set_cap(exb, str, needed + 1);
        if (rv != EXB_OK) {
            return rv;
        }
        exb_assert_s(needed < str->cap, "str grow failed");
        va_copy(ap, ap_in);
        needed = vsnprintf(str->str, str->cap, fmt, ap);
        exb_assert_s(needed < str->cap, "str grow failed");
        va_end(ap);
    }
    str->len = needed;
    return EXB_OK;
}
static int exb_sprintf(struct exb *exb, struct exb_str *str, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int rv = exb_vsprintf(exb, str, fmt, ap);
    va_end(ap);
    return rv;
}


#endif //EXB_STR_H
