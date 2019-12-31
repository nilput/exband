#include "cpb.h"
#include "errors.h"
#include <string.h>
#include <stdlib.h>

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

//see str valid states at kdata.h
static int cpb_str_init(struct cpb *cpb, struct cpb_str *str) {
    (void) cpb;
    str->str = "";
    str->cap = -1;
    str->len = 1;
    return CPB_OK;
}


//src0 must outlive the object (doesn't own it)
static int cpb_str_init_const_str(struct cpb *cpb, struct cpb_str *str, const char *src0) {
    (void) cpb;
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

static int cpb_str_new(struct cpb *cpb, struct cpb_str **strp) {
    void *p = NULL;
    int rv = cpb_malloc(cpb, sizeof(struct cpb_str), &p);
    if (rv != CPB_OK) {
        *strp = NULL;
        return rv;
    }
    rv = cpb_str_init(cpb, p);
    *strp = p;
    return rv;
}
static int cpb_str_destroy(struct cpb *cpb, struct cpb_str *strp) {
    int rv = cpb_str_deinit(cpb, strp);
    int rv2 = cpb_free(cpb, strp);
    if (rv != CPB_OK)
        return rv;
    return rv2;
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
    if (str->cap < 0) {
        void *p;
        int rv  = cpb_malloc(cpb, capacity, &p);
        if (rv != CPB_OK) {
            return rv;
        }
        memcpy(p, str->str, str->len);
        str->str = p;
        str->str[str->len] = 0;
    }
    else {
        void *p = NULL;
        int rv = cpb_realloc(cpb, str->str, capacity, &p);
        if (rv != CPB_OK) {
            return rv;
        }
        str->str = p;
    }
    str->cap = capacity;
    return CPB_OK;
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
    str->str[str->len] = 0;
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
    int rv  = cpb_malloc(cpb, len + 1, &p); CPB_CRV(rv);
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
//assumes the length of both was checked and it was equal!
static int cpb_strl_eq(const char *a, const char *b, size_t len) {
    return memcmp(a, b, len) == 0;
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

//boolean
static int cpb_str_streq(struct cpb *cpb, struct cpb_str *str_a, struct cpb_str *str_b) {
    if (str_a->len != str_b->len)
        return 0;
    else if (str_a->len == 0)
        return 1;
    return cpb_strl_eq(str_a->str, str_b->str, str_a->len);
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