#ifndef EXB_STR_H
#define EXB_STR_H
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "exb.h"
#include "exb_errors.h"
#define EXB_CRV(code)

enum EXB_STR_FLAGS {
    EXB_STR_DYNAMIC = 1, //backed by malloc?
    EXB_STR_CONST   = 2, //can overwrite?
};
/*
string can:
    be const if flags & EXB_STR_CONST, shouldn't be written to
    be backed by local storage if !(flags & EXB_STR_DYNAMIC)
*/
struct exb_str {
    char *str; //null terminated
    int len;
    int zcap; //negative values mean memory is not owned by us (const char * passed to us)
    unsigned char flags; //backed by malloc?
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
    s.zcap = 0;
    s.flags = EXB_STR_CONST;
    return s;
}
static int exb_str_is_const(struct exb_str *str) {
    return str->flags & EXB_STR_CONST;
}
static int exb_str_is_writable(struct exb_str *str) {
    return !exb_str_is_const(str);
}
static int exb_str_is_growable(struct exb_str *str) {
    return (str->flags & (EXB_STR_CONST | EXB_STR_DYNAMIC)) == EXB_STR_DYNAMIC;
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
    while (str->len > 0 && (str->str[str->len - 1] == ' ' ||
                            str->str[str->len - 1] == '\t'  ))
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
    view.flags = EXB_STR_CONST;
    return view;
}

static int exb_str_init_empty(struct exb_str *str) {
    str->str = "";
    str->zcap = 0;
    str->len = 0;
    str->flags = EXB_STR_CONST;
    return EXB_OK;
}

//initializes a string backed by a char array on the stack
//this will expand as needed to dynamic memory
//Note: you still have to call exb_str_deinit, in case the string grew, otherwise it will be a no-op
static int exb_str_init_empty_by_local_buffer(struct exb_str *str, char *buffer, size_t size) {
    exb_assert_h(!!buffer, "passed NULL buffer");
    buffer[0] = 0;
    str->str = buffer;
    str->len = 0;
    str->zcap = size;
    str->flags = 0;
    return EXB_OK;
}

//src0 must outlive the object (doesn't own it)
static int exb_str_init_const_str(struct exb_str *str, const char *src0) {
    exb_assert_h(!!src0, "passed NULL string");
    str->str = (char *) src0;
    str->len = strlen(src0);
    str->zcap = 0;
    str->flags = EXB_STR_CONST;
    return EXB_OK;
}

static int exb_str_deinit(struct exb *exb, struct exb_str *str) {
    if ((str->flags & (EXB_STR_CONST | EXB_STR_DYNAMIC)) == EXB_STR_DYNAMIC) {
        exb_free(exb, str->str);
    }
    #ifdef EXB_DEBUG
        str->str = NULL;
        str->zcap = 0;
        str->len = 0;
    #endif
    return EXB_OK;
}

static int exb_str_strlcpy(struct exb *exb, struct exb_str *str, const char *src, int srclen);

static int exb_str_init_strlcpy(struct exb *exb, struct exb_str *str, const char *src, int src_len) {
    exb_str_init_empty(str);
    int rv = exb_str_strlcpy(exb, str, src, src_len);
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
    exb_str_init_empty(p);
    *strp = p;
    return EXB_OK;
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
    if (exb_str_is_const(str)) {
        str->str = "";
        str->len = 0;
        str->zcap = 0;
    }
    else {
        str->str[0] = 0;
        str->len = 0;
    }
    return EXB_OK;
}
static int exb_str_set_cap(struct exb *exb, struct exb_str *str, int capacity) {
    if (capacity <= 0) {
        return exb_str_clear(exb, str);
    }
    int newlen = str->len;
    if (capacity <= str->len)
        newlen = capacity - 1;
    void *p;
    if (!exb_str_is_growable(str)) {
        p  = exb_malloc(exb, capacity);
        if (!p) {
            return EXB_NOMEM_ERR;
        }
        memcpy(p, str->str, str->len);
    }
    else {
        void *p = NULL;
        p = exb_realloc(exb, str->str, capacity);
        if (!p) {
            return EXB_NOMEM_ERR;
        }
    }
    str->str = p;
    str->len = newlen;
    str->str[str->len] = 0;
    str->zcap = capacity;
    return EXB_OK;
}
//make sure capacity is at least the provided arg
static int exb_str_ensure_cap(struct exb *exb, struct exb_str *str, int capacity) {
    if (str->zcap >= capacity) {
        return EXB_OK;
    }
    if (capacity < 16)
        capacity = 16;
    return exb_str_set_cap(exb, str, capacity);
}
static int exb_str_strlcpy(struct exb *exb, struct exb_str *str, const char *src, int srclen) {
    int rv;
    if (str->zcap <= srclen) {
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

//boolean return value
static int exb_strc_endswith(const char *str, const char *suffix) {
    int slen = strlen(str);
    int suffixlen = strlen(suffix);
    return (slen > suffixlen) && (strcmp(str+slen-suffixlen, suffix) == 0);
}
static int exb_str_strlappend(struct exb *exb, struct exb_str *str, const char *src, int srclen) {
    int rv;
    if (exb_str_is_const(str) || str->zcap <= (str->len + srclen)) {
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
    exb_str_init_empty(str);
    int rv = exb_str_strlcpy(exb, str, src->str, src->len);
    if (rv != EXB_OK) {
        exb_str_deinit(exb, str);
        return rv;
    }
    return EXB_OK;
}
static int exb_str_copy(struct exb *exb, struct exb_str *str, struct exb_str *src) {
    return exb_str_strlcpy(exb, str, src->str, src->len);
}
//transfer ownership, string must be allocated with exb_malloc
static int exb_str_init_transfer(struct exb *exb, char *src_str, struct exb_str *dest) {
    exb_str_init_empty(dest);
    dest->len = strlen(src_str);
    dest->zcap = dest->len;
    dest->flags = EXB_STR_DYNAMIC;
    dest->str = src_str;
    return EXB_OK;
}
//destroys dest
static int exb_str_assign_transfer(struct exb *exb, char *src_str, struct exb_str *dest) {
    exb_str_deinit(exb, dest);
    return exb_str_init_transfer(exb, src_str, dest);
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
    exb_str_init_empty(str);
    int rv = exb_str_strcpy(exb, str, src0);
    if (rv != EXB_OK) {
        exb_str_deinit(exb, str);
        return rv;
    }
    return EXB_OK;
}


static int exb_vsprintf(struct exb *exb, struct exb_str *str, const char *fmt, va_list ap_in) {
    va_list ap;
    int needed;

    va_copy(ap, ap_in);
    if (exb_str_is_const(str) || str->zcap < 2) {
        char tmp_buff[2];
        needed = vsnprintf(tmp_buff, sizeof tmp_buff, fmt, ap);
    }
    else {
        needed = vsnprintf(str->str, str->zcap, fmt, ap);
    }
    va_end(ap);

    if (needed >= str->zcap || exb_str_is_const(str)) {
        int rv = exb_str_set_cap(exb, str, needed + 1);
        if (rv != EXB_OK) {
            return rv;
        }
        exb_assert_s(needed < str->zcap, "str grow failed");
        va_copy(ap, ap_in);
        needed = vsnprintf(str->str, str->zcap, fmt, ap);
        exb_assert_s(needed < str->zcap, "str grow failed");
        va_end(ap);
    }
    str->len = needed;
    return EXB_OK;
}

/*

*/
static int exb_sprintf(struct exb *exb, struct exb_str *str, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int rv = exb_vsprintf(exb, str, fmt, ap);
    va_end(ap);
    return rv;
}


#endif //EXB_STR_H
