#ifndef EXB_UTILS_H
#define EXB_UTILS_H

int exb_sleep(int ms);
int exb_memmem(const char *haystack, int hidx, int hlen, const char *needle, int nlen);
int exb_memchr(const char *haystack, int hidx, int hlen, char needle);
int exb_itoa(char *dest, int dest_size, int *written, int num);
int exb_atoi(char *str, int len, int *dest);
int exb_atoi_hex(char *str, int len, int *dest);
//returns length of read bytes, 0 means error
int exb_atoi_hex_rlen(char *str, int len, int *dest);
//assumes str is initialized
struct exb_str;
struct exb;
int exb_str_itoa(struct exb *exb, struct exb_str *str, int num);
double exb_time();

#endif// EXB_UTILS_H
