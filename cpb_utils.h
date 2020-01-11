#ifndef CPB_UTILS_H
#define CPB_UTILS_H

int cpb_sleep(int ms);
int cpb_memmem(const char *haystack, int hidx, int hlen, const char *needle, int nlen);
int cpb_itoa(char *dest, int dest_size, int *written, int num);
int cpb_atoi(char *str, int len, int *dest);
int cpb_atoi_hex(char *str, int len, int *dest);
//returns length of read bytes, 0 means error
int cpb_atoi_hex_rlen(char *str, int len, int *dest);
//assumes str is initialized
struct cpb_str;
struct cpb;
int cpb_str_itoa(struct cpb *cpb, struct cpb_str *str, int num);
double cpb_time();

#endif// CPB_UTILS_H
