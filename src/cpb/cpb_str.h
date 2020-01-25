#ifndef CPB_STR_H
#define CPB_STR_H
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "cpb.h"
#include "cpb_errors.h"
#define CPB_CRV(code)

/*
* valid states:
*   .len > 0 :   memory is owned by the object
*   .len == -1 : memory is read only, and not owned by the object
*   in both cases .len and .str are valid, .str points to the empty string if the string is just initialized
*/
struct cpb_str {
    char *str; //null terminated
    int len;
    int cap; //negative values mean memory is not owned by us (const char * passed to us)
};
struct cpb_str_slice {
    int index;
    int len;
};


//returned str is not null terminated!
static struct cpb_str cpb_str_slice_to_const_str(struct cpb_str_slice slice, const char *base) {
    struct cpb_str s;
    s.str = (char *)base + slice.index;
    s.len = slice.len;
    s.cap = -1;
    return s;
}
static int cpb_str_is_const(struct cpb_str *str) {
    return str->cap == -1;
}

static int cpb_str_init_strlcpy(struct cpb *cpb, struct cpb_str *str, const char *src, int src_len);
static int cpb_str_strlcpy(struct cpb *cpb, struct cpb_str *str, const char *src, int src_len);

//assumes str is already initialized
static int cpb_str_slice_to_copied_str(struct cpb *cpb, struct cpb_str_slice slice, const char *base, struct cpb_str *out) {
    int rv = cpb_str_strlcpy(cpb, out, base + slice.index, slice.len);
    if (rv != CPB_OK)
        return rv;
    return CPB_OK;
}


static int cpb_str_clone(struct cpb *cpb, struct cpb_str *str) {
    return cpb_str_init_strlcpy(cpb, str, str->str, str->len);
}

static int cpb_str_rtrim(struct cpb *cpb, struct cpb_str *str) {
    int rv;
    if (cpb_str_is_const(str) && ((rv = cpb_str_clone(cpb, str) != CPB_OK))) {
        return rv;
    }
    while (str->len > 0 && (str->str[str->len-1] == ' ' ||
                            str->str[str->len-1] == '\t'  ))
        str->len--;
    str->str[str->len] = 0;
    return CPB_OK;
}
static void cpb_str_slice_trim(const char *base, struct cpb_str_slice *slice) {
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

static struct cpb_str cpb_str_const_view(struct cpb_str *str) {
    struct cpb_str view = *str;
    view.cap = -1;
    return view;
}


//see str valid states at kdata.h
static int cpb_str_init(struct cpb *cpb, struct cpb_str *str) {
    (void) cpb;
    str->str = "";
    str->cap = -1;
    str->len = 0;
    return CPB_OK;
}

static int cpb_str_init_empty(struct cpb_str *str) {
    str->str = "";
    str->cap = -1;
    str->len = 0;
    return CPB_OK;
}

//src0 must outlive the object (doesn't own it)
static int cpb_str_init_const_str(struct cpb_str *str, const char *src0) {
    cpb_assert_h(!!src0, "passed NULL string");
    str->str = (char *) src0;
    str->len = strlen(src0);
    str->cap = -1;
    return CPB_OK;
}

static int cpb_str_deinit(struct cpb *cpb, struct cpb_str *str) {
    if (str->cap >= 0) {
        cpb_free(cpb, str->str);
    }
    str->str = NULL;
    str->cap = 0;
    str->len = 0;
    return CPB_OK;
}

static int cpb_str_strlcpy(struct cpb *cpb, struct cpb_str *str, const char *src, int srclen);

static int cpb_str_init_strlcpy(struct cpb *cpb, struct cpb_str *str, const char *src, int src_len) {
    int rv = cpb_str_init(cpb, str);
    if (rv != CPB_OK)
        return rv;
    rv = cpb_str_strlcpy(cpb, str, src, src_len);
    if (rv != CPB_OK) {
        cpb_str_deinit(cpb, str);
        return rv;
    }
    return CPB_OK;
}

static int cpb_str_new(struct cpb *cpb, struct cpb_str **strp) {
    void *p = NULL;
    p = cpb_malloc(cpb, sizeof(struct cpb_str));
    if (!p) {
        *strp = NULL;
        return CPB_NOMEM_ERR;
    }
    int rv = cpb_str_init(cpb, p);
    *strp = p;
    return rv;
}
static int cpb_str_destroy(struct cpb *cpb, struct cpb_str *strp) {
    int rv = cpb_str_deinit(cpb, strp);
    cpb_free(cpb, strp);

    return rv;
}
static int cpb_str_strcpy(struct cpb *cpb, struct cpb_str *str, const char *src0);
static int cpb_str_new_strcpy(struct cpb *cpb, struct cpb_str **strp, const char *src0) {
    int rv = cpb_str_new(cpb, strp); CPB_CRV(rv);
    rv = cpb_str_strcpy(cpb, *strp, src0);
    if (rv != CPB_OK) {
        cpb_str_destroy(cpb, *strp);
        *strp = NULL;
        return rv;
    }
    return CPB_OK;
}


static int cpb_str_clear(struct cpb *cpb, struct cpb_str *str) {
    (void) cpb;
    if (str->cap > 0) {
        str->str[0] = 0;
        str->len = 0;
    }
    else {
        str->str = "";
        str->len = 0;
        str->cap = -1;
    }
    return CPB_OK;
}
static int cpb_str_set_cap(struct cpb *cpb, struct cpb_str *str, int capacity) {
    if (capacity == 0) {
        return cpb_str_clear(cpb, str);
    }
    if (capacity < (str->len + 1))
        str->len = capacity - 1;
    if (str->cap < 0) {
        void *p;
        p  = cpb_malloc(cpb, capacity);
        if (!p) {
            return CPB_NOMEM_ERR;
        }
        memcpy(p, str->str, str->len);
        str->str = p;
    }
    else {
        void *p = NULL;
        p = cpb_realloc(cpb, str->str, capacity);
        if (!p) {
            return CPB_NOMEM_ERR;
        }
        str->str = p;
    }
    str->str[str->len] = 0;
    str->cap = capacity;
    return CPB_OK;
}
//make sure capacity is at least the provided arg
static int cpb_str_ensure_cap(struct cpb *cpb, struct cpb_str *str, int capacity) {
    if (str->cap >= capacity) {
        return CPB_OK;
    }
    if (capacity < 16)
        capacity = 16;
    return cpb_str_set_cap(cpb, str, (capacity * 3) / 2);
}
static int cpb_str_strlcpy(struct cpb *cpb, struct cpb_str *str, const char *src, int srclen) {
    int rv;
    if (str->cap <= srclen) {
        rv = cpb_str_set_cap(cpb, str, srclen + 1);
        if (rv != CPB_OK) {
            return rv;
        }
    }
    memcpy(str->str, src, srclen);
    str->len = srclen;
    str->str[srclen] = 0;
    return CPB_OK;
}
static int cpb_str_strlappend(struct cpb *cpb, struct cpb_str *str, const char *src, int srclen) {
    int rv;
    if (str->cap <= (str->len + srclen)) {
        rv = cpb_str_set_cap(cpb, str, str->len + srclen + 1);
        if (rv != CPB_OK) {
            return rv;
        }
    }
    memcpy(str->str + str->len, src, srclen);
    str->len += srclen;
    str->str[str->len] = 0;
    return CPB_OK;
}
static int cpb_str_charappend(struct cpb *cpb, struct cpb_str *str, unsigned char ch) {
    char buff[1] = {ch};
    return cpb_str_strlappend(cpb, str, buff, 1);
}
static int cpb_str_strcpy(struct cpb *cpb, struct cpb_str *str, const char *src0) {
    return cpb_str_strlcpy(cpb, str, src0, strlen(src0));
}
static int cpb_str_strappend(struct cpb *cpb, struct cpb_str *str, const char *src0) {
    return cpb_str_strlappend(cpb, str, src0, strlen(src0));
}
static int cpb_str_init_copy(struct cpb *cpb, struct cpb_str *str, struct cpb_str *src) {
    int rv = cpb_str_init(cpb, str); CPB_CRV(rv);
    rv = cpb_str_strlcpy(cpb, str, src->str, src->len);
    if (rv != CPB_OK) {
        cpb_str_deinit(cpb, str);
        return rv;
    }
    return CPB_OK;
}
static int cpb_str_new_copy(struct cpb *cpb, struct cpb_str **strp, struct cpb_str *src) {
    int rv = cpb_str_new(cpb, strp);
    if (rv != CPB_OK) {
        return rv;
    }
    rv = cpb_str_strlcpy(cpb, *strp, src->str, src->len);
    if (rv != CPB_OK) {
        cpb_str_destroy(cpb, *strp);
        *strp = NULL;
    }
    return CPB_OK;
}
//exclusive end, inclusive begin
static int cpb_str_mutsubstr(struct cpb *cpb, struct cpb_str *str, int begin, int end) {
    cpb_assert_h((begin <= end) && (begin <= str->len) && (end <= str->len), "invalid arguments to mutsubstr()");
    void *p = NULL;
    int len = end - begin;
    p = cpb_malloc(cpb, len + 1);
    if (!p)
        return CPB_NOMEM_ERR;
    memcpy(p, str->str + begin, len);
    cpb_free(cpb, str->str);
    str->str = p;
    str->len = len;
    str->str[len] = 0;
    return CPB_OK;
}
enum cpb_str_mutstrip {
    CPB_STRIP_DEFAULT = 0,
    CPB_STRIP_LEFT = 1,
    CPB_STRIP_RIGHT = 2,
};
//stripchars are treated like a set, each assumed to be one char to be excluded repeatedly from beginning and end
static int cpb_str_mutstrip(struct cpb *cpb, struct cpb_str *str, const char *stripchars, enum cpb_str_mutstrip opts) {
    if (!opts)
        opts = CPB_STRIP_LEFT + CPB_STRIP_RIGHT;
    int begin = 0;
    int end = str->len;
    if (opts & CPB_STRIP_LEFT) {
        for (int i=0; i < str->len && strchr(stripchars, str->str[i]); i++)
            begin++;
    }
    if (opts & CPB_STRIP_RIGHT) {
        for (int i=str->len - 1; i > begin && strchr(stripchars, str->str[i]); i--)
            end--;
    }
    if (begin == 0 && end == str->len)
        return CPB_OK;
    return cpb_str_mutsubstr(cpb, str, begin, end);
}

static int cpb_strl_eq(const char *a, size_t alen, const char *b, size_t blen) {
    return alen == blen && (memcmp(a, b, alen) == 0);
}

//doesnt work on binary strings
static int cpb_strcasel_eq(const char *a, size_t alen, const char *b, size_t blen) {
    return alen == blen && (strncasecmp(a, b, alen) == 0);
}
//boolean
static int cpb_str_streqc(struct cpb *cpb, struct cpb_str *str, const char *src0) {
    if (!str->len && !src0[0])
        return 1;
    if (!str->len || !src0[0])
        return 0;
    cpb_assert_h(!!str->str, "invalid string passed to be compared");
    return strcmp(str->str, src0) == 0;
}
static int cpb_str_startswithc(struct cpb *cpb, struct cpb_str *str, const char *src0) {
    cpb_assert_h(str->str && src0, "invalid string passed to be compared");
    return strncmp(str->str, src0, strlen(src0)) == 0;
}

//boolean
static int cpb_str_streq(struct cpb *cpb, struct cpb_str *str_a, struct cpb_str *str_b) {
    if (str_a->len != str_b->len)
        return 0;
    else if (str_a->len == 0)
        return 1;
    return cpb_strl_eq(str_a->str, str_a->len, str_b->str, str_a->len);
}


static int cpb_str_init_strcpy(struct cpb *cpb, struct cpb_str *str, const char *src0) {
    int rv = cpb_str_init(cpb, str);
    if (rv != CPB_OK)
        return rv;
    rv = cpb_str_strcpy(cpb, str, src0);
    if (rv != CPB_OK) {
        cpb_str_deinit(cpb, str);
        return rv;
    }
    return CPB_OK;
}


static int cpb_vsprintf(struct cpb *cpb, struct cpb_str *str, const char *fmt, va_list ap_in) {
    int rv;
    va_list ap;
    if (str->cap < 2) {
        rv = cpb_str_set_cap(cpb, str, 2);
        if (rv != CPB_OK) {
            return rv;
        }
    }
    va_copy(ap, ap_in);
    int needed = vsnprintf(str->str, str->cap, fmt, ap);
    va_end(ap);
    if (needed >= str->cap) {
        rv = cpb_str_set_cap(cpb, str, needed + 1);
        if (rv != CPB_OK) {
            return rv;
        }
        cpb_assert_s(needed < str->cap, "str grow failed");
        va_copy(ap, ap_in);
        needed = vsnprintf(str->str, str->cap, fmt, ap);
        cpb_assert_s(needed < str->cap, "str grow failed");
        va_end(ap);
    }
    str->len = needed;
    return CPB_OK;
}
static int cpb_sprintf(struct cpb *cpb, struct cpb_str *str, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int rv = cpb_vsprintf(cpb, str, fmt, ap);
    va_end(ap);
    return rv;
}


#endif //CPB_STR_H
