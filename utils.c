#include "utils.h"
#include <unistd.h>
#include <sys/time.h>
#include <string.h>
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
        return 0;
    if ((hlen - hidx) <= 0)
        return -1;
    char *f = memchr(haystack + hidx, needle[0], hlen - hidx);
    while (f != NULL) {
        int f_idx = f - haystack;
        if (hlen - f_idx < nlen)
            return -1;
        if (memcmp(f, needle, nlen) == 0) {
            return f_idx;
        }
        hidx = f_idx + 1;
        f = memchr(haystack + hidx, needle[0], hlen - hidx);
    }
    return -1;
}
