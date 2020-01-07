#ifndef CPB_UTILS_H
#define CPB_UTILS_H

int cpb_sleep(int ms);
int cpb_memmem(const char *haystack, int hidx, int hlen, const char *needle, int nlen);
int cpb_itoa(char *dest, int dest_size, int *written, int num);
//assumes str is initialized
struct cpb_str;
struct cpb;
int cpb_str_itoa(struct cpb *cpb, struct cpb_str *str, int num);
double cpb_time();

#endif// CPB_UTILS_H
