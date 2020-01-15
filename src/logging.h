#include <stdarg.h>
#include "cpb.h"
#include "cpb_errors.h"
#include "cpb_str.h"
struct cpb_logger loggers[];
struct cpb_logger {
    struct cpb *cpb; /*not owned, must outlive*/
    int level;
};

int cpb_logger_logvf(struct cpb_logger *logger, const char *fmt, va_list ap_in) {
    va_list ap;
    va_copy(ap, ap_in);
    char tbuf[2];
    int needed = vsnprintf(tbuf, 2, fmt, ap);
    va_end(ap);
    struct cpb_str str;
    cpb_str_init(logger->cpb, &str);
    if (needed >= str.cap) {
        int rv = cpb_str_set_cap(logger->cpb, &str, needed + 1);
        if (rv != CPB_OK) {
            return rv;
        }
        cpb_assert_s(needed < str.cap, "str grow failed");
        va_copy(ap, ap_in);
        needed = vsnprintf(str.str, str.cap, fmt, ap);
        cpb_assert_s(needed < str.cap, "str grow failed");
        va_end(ap);
    }
    str.len = needed;
    cpb_str_deinit(logger->cpb, &str);
    return CPB_OK;
}
static int cpb_logger_logf(struct cpb_logger *logger, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int rv = cpb_logger_logvf(logger,fmt, ap);
    va_end(ap);
    return rv;
}