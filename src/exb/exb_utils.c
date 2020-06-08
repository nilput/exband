#define _GNU_SOURCE
#include "exb_utils.h"
#include "exb.h"
#include "exb_errors.h"
#include "exb_str.h"
#include "exb_build_config.h"
#include <unistd.h>
#include <sys/time.h>

#include <string.h>
#include <stdio.h>
int exb_sleep(int ms) {
    return usleep(ms * 1000);
}
int exb_sleep_until_timestamp_ex(struct exb_timestamp now, struct exb_timestamp target_timestamp) {
    long long usec = exb_timestamp_get_as_usec(exb_timestamp_diff(target_timestamp, now));
    if (usec <= 0)
        return 0;
    return usleep(usec);
}
int exb_sleep_until_timestamp(struct exb_timestamp ts) {
    return exb_sleep_until_timestamp_ex(exb_timestamp_now(), ts);   
}
//returns unix time in seconds with at least ms accuarcy
double exb_time() {
    struct timeval tv;
    int rv = gettimeofday(&tv, NULL);
    if (rv != 0)
        return 0;
    return tv.tv_sec + (tv.tv_usec / 1000000.0);
}
//returns unix time in seconds
struct exb_timestamp exb_timestamp_now() {
    struct timeval tv;
    int rv = gettimeofday(&tv, NULL);
    if (rv != 0)
        return exb_timestamp(0);
    return exb_timestamp(tv.tv_sec);
}
int exb_memchr(const char *haystack, int hidx, int hlen, char needle) {
    char *f = memchr(haystack + hidx, needle, hlen - hidx);
    if (f != NULL)
        return f - haystack;
    return -1;
}

int exb_memmem(const char *haystack, int hidx, int hlen, const char *needle, int nlen) {
    if (nlen == 0)
        return hidx;
    if (hidx + nlen > hlen)
        return -1;
#ifdef USE_GNU_MEMMEM
    void *f = memmem(haystack + hidx, hlen - hidx, needle, nlen);
    if (f == NULL)
        return -1;
    return f - (void *)haystack;
#else
    char *f = memchr(haystack + hidx, needle[0], hlen - hidx);
    while (f != NULL) {
        int f_idx = f - haystack;
        if (f_idx + nlen > hlen)
            return -1;
        if (memcmp(f, needle, nlen) == 0) {
            return f_idx;
        }
        hidx = f_idx + 1;
        f = memchr(haystack + hidx, needle[0], hlen - hidx);
    }
    return -1;
#endif
}

int exb_itoa(char *dest, int dest_size, int *written_out, int num) {
#define EXB_ITOA_MAX_DIGITS 20
    char digits[EXB_ITOA_MAX_DIGITS];
    int ndigits = 0;
    int neg = num < 0;
    num = num < 0 ? - num : num;
    for (int i=0; i<EXB_ITOA_MAX_DIGITS; i++) {
        digits[i] = '0' + (num % 10);
        num /= 10;
        if (num == 0) {
            ndigits = i + 1;
            break;
        }
    }
    if ((ndigits + neg + 1) > dest_size) {
        *written_out = 0;
        *dest = '\0';
        return EXB_OUT_OF_RANGE_ERR;
    }
    *written_out= ndigits + neg;
    if (neg)
        *(dest++) = '-';
    for (int i=0; i<ndigits; i++) {
        dest[i] = digits[ndigits - i - 1];
    }
    return EXB_OK;
}
int exb_atoi(char *str, int len, int *dest) {
    char *end = NULL;
    long value = strtol(str, &end, 10);
    if (end == str) {
        *dest = 0;
        return EXB_INVALID_INT_ERR;
    }
    *dest = value;
    return EXB_OK;
}
int exb_atoi_hex(char *str, int len, int *dest) {
    char *end = NULL;
    long value = strtol(str, &end, 16);
    if (end == str) {
        *dest = 0;
        return EXB_INVALID_INT_ERR;
    }
    *dest = value;
    return EXB_OK;
}
//returns length of read bytes, 0 means error
int exb_atoi_hex_rlen(char *str, int len, int *dest) {
    char *end = NULL;
    long value = strtol(str, &end, 16);
    if (end == str) {
        *dest = 0;
        return 0;
    }
    *dest = value;
    return end - str;
}
int exb_str_itoa(struct exb *exb, struct exb_str *str, int num) {
    if (exb_str_is_const(str) || str->zcap < EXB_INT_DIGITS) {
        int rv = exb_str_set_cap(exb, str, EXB_INT_DIGITS);
        if (rv != EXB_OK)
            return rv;
    }
    return exb_itoa(str->str, str->zcap, &str->len, num);
}

int exb_read_file_fully(struct exb *exb, FILE *f, size_t max_sz, char **buffer_out, size_t *sz_out) {
    size_t did_read = 0;
    size_t buffsz = 0;
    char *buff = NULL;

    *buffer_out = NULL;
    *sz_out = 0;
    while (!ferror(f) && !feof(f)) {
        if ((buffsz - did_read) == 0) {
            if (buffsz == 0)
                buffsz = 1024;
            else
                buffsz *= 2;
            if (buffsz > max_sz) {
                free(buff);
                return EXB_OUT_OF_RANGE_ERR;
            }
            buff = exb_realloc_f(exb, buff, buffsz);
        }
        did_read += fread(buff + did_read, 1, buffsz - did_read, f);
    }
    if (ferror(f)) {
        free(buff);
        return EXB_READ_ERR;
    }
    if (did_read == buffsz) {
       buff = exb_realloc_f(exb, buff, did_read + 1);
       if (!buff) {
           return EXB_OUT_OF_RANGE_ERR;
       }
    }
    *sz_out = did_read;
    *buffer_out = buff;
    buff[did_read] = 0;
    return EXB_OK;
}


#ifdef EXB_TRACE
#define _GNU_SOURCE
#include <dlfcn.h>
void __cyg_profile_func_enter(void *this_fn, void *call_site) __attribute__((no_instrument_function));
void __cyg_profile_func_exit(void *this_fn, void *call_site) __attribute__((no_instrument_function));
void common_func(void* func_address, const char *fmt) __attribute__((no_instrument_function));
const char* addr2name(void* address) __attribute__((no_instrument_function));
double exb_time() __attribute__((no_instrument_function));
void __cyg_profile_func_enter(void* func_address, void* call_site) {
    common_func( func_address, "enter =>" );
}

void __cyg_profile_func_exit (void* func_address, void* call_site) {
    common_func( func_address, "<= exit" );
}

//credits: https://github.com/tomohikoseven/-finstrument-functions/blob/master/trace.c
const char* addr2name(void* address) {
    Dl_info dli;
    memset(&dli, 0x00, sizeof dli);

    if (dladdr(address, &dli)) {
        return dli.dli_sname;
    }
    return NULL;
}

void common_func(void* func_address, const char *fmt) {
    const char *ret = addr2name(func_address);
    if (ret) {
        printf("%s %s %f\n", fmt, ret, exb_time());
    }
}

#endif
