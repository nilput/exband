#ifndef EXB_UTILS_H
#define EXB_UTILS_H
#include <stdio.h>
#include "exb_time.h"

int exb_sleep(int ms);
int exb_sleep_until_timestamp(struct exb_timestamp ts);
int exb_sleep_until_timestamp_ex(struct exb_timestamp now, struct exb_timestamp target_timestamp);
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
struct exb_timestamp exb_timestamp_now();

//Does not close file!
int exb_read_file_fully(struct exb *exb, FILE *f, size_t max_s, char **buff, size_t *sz);

#endif// EXB_UTILS_H
