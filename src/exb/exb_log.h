#ifndef EXB_LOG_H
#define EXB_LOG_H

#include <stdarg.h>
#include "exb.h"
#include "exb_errors.h"
#include "exb_str.h"

enum EXB_LOG_LEVEL {
    EXB_LOG_NONE,
    EXB_LOG_ERROR,
    EXB_LOG_INFO,
    EXB_LOG_DEBUG,
};

static int exb_logger_logvf(struct exb *exb, int level, const char *fmt, va_list ap_in) {
    va_list ap;
    va_copy(ap, ap_in);
    char tbuf[2];
    int needed = vsnprintf(tbuf, 2, fmt, ap);
    va_end(ap);
    struct exb_str str;
    exb_str_init_empty(&str);
    if (needed >= str.zcap || exb_str_is_const(&str)) {
        int rv = exb_str_set_cap(exb, &str, needed + 1);
        if (rv != EXB_OK) {
            return rv;
        }
        exb_assert_s(needed < str.zcap, "str grow failed");
        va_copy(ap, ap_in);
        needed = vsnprintf(str.str, str.zcap, fmt, ap);
        exb_assert_s(needed < str.zcap, "str grow failed");
        va_end(ap);
    }
    str.len = needed;
    fprintf(stderr, "%s", str.str);
    exb_str_deinit(exb, &str);
    return EXB_OK;
}
static int exb_logger_logf(struct exb *exb, int level, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int rv = exb_logger_logvf(exb, level, fmt, ap);
    va_end(ap);
    return rv;
}
static int exb_log_error(struct exb *exb, const char *fmt, ...)  {
    va_list ap;
    va_start(ap, fmt);
    int rv = exb_logger_logvf(exb, EXB_LOG_ERROR, fmt, ap);
    va_end(ap);
    return rv;
}

#endif