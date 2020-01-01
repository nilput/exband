#ifndef CPB_UTILS_H
#define CPB_UTILS_H

int cpb_sleep(int ms);
int cpb_memmem(const char *haystack, int hidx, int hlen, const char *needle, int nlen);
double cpb_time();

#endif// CPB_UTILS_H