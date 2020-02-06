#define _GNU_SOURCE
#include "cpb_utils.h"
#include "cpb_errors.h"
#include "cpb_str.h"
#include <unistd.h>
#include <sys/time.h>

#include <string.h>
#include <stdio.h>
int cpb_sleep(int ms) {
    return usleep(ms * 1000);
}
//returns unix time in seconds with at least ms accuarcy
double cpb_time() {
    struct timeval tv;
    int rv = gettimeofday(&tv, NULL);
    if (rv != 0)
        return 0;
    return tv.tv_sec + (tv.tv_usec / 1000000.0);
}
int cpb_memmem(const char *haystack, int hidx, int hlen, const char *needle, int nlen) {
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

struct itoar {
    int err;
    int rem;
};
static struct itoar itoar(int err, int rem) {
    return (struct itoar){err, rem};
}
struct itoar cpb_itoa_1(char *dest, int dest_size, int written, int *written_out, int num) {
    if (num == 0) {
        return itoar(0, 0);
    }
    if (dest_size < 2) {
        return itoar(CPB_OUT_OF_RANGE_ERR, 0);
    }
    struct itoar next = cpb_itoa_1(dest+1, dest_size-1, written + 1, num);
}
int cpb_itoa(char *dest, int dest_size, int *written_out, int num) {
    if (!dest_size) {
        return CPB_OUT_OF_RANGE_ERR;
    }
    if (num == 0) {
    *dest = '0';
    *written_out = 0;
    return CPB_OK;
    }
    int written = 0;
    if (num < 0) {
        written++;
        *dest = '-';
        dest++;
        dest_size--;
        num = -num;
    }
    return cpb_itoa_1(dest, dest_size, 0, written, num).err;
}
int cpb_atoi(char *str, int len, int *dest) {
    char *end = NULL;
    long value = strtol(str, &end, 10);
    if (end == str) {
        *dest = 0;
        return CPB_INVALID_INT_ERR;
    }
    *dest = value;
    return CPB_OK;
}
int cpb_atoi_hex(char *str, int len, int *dest) {
    char *end = NULL;
    long value = strtol(str, &end, 16);
    if (end == str) {
        *dest = 0;
        return CPB_INVALID_INT_ERR;
    }
    *dest = value;
    return CPB_OK;
}
//returns length of read bytes, 0 means error
int cpb_atoi_hex_rlen(char *str, int len, int *dest) {
    char *end = NULL;
    long value = strtol(str, &end, 16);
    if (end == str) {
        *dest = 0;
        return 0;
    }
    *dest = value;
    return end - str;
}
int cpb_str_itoa(struct cpb *cpb, struct cpb_str *str, int num) {
    if (str->cap < 32) {
        int rv = cpb_str_set_cap(cpb, str, 32);
        if (rv != CPB_OK)
            return rv;
    }
    return cpb_itoa(str->str, str->cap, &str->len, num);
}
