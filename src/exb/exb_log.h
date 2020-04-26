#include <stdarg.h>
#include "exb.h"
#include "exb_errors.h"
#include "exb_str.h"

static int exb_logger_logvf(struct exb *exb, int level, const char *fmt, va_list ap_in) {
    va_list ap;
    va_copy(ap, ap_in);
    char tbuf[2];
    int needed = vsnprintf(tbuf, 2, fmt, ap);
    va_end(ap);
    struct exb_str str;
    exb_str_init(exb, &str);
    if (needed >= str.cap) {
        int rv = exb_str_set_cap(exb, &str, needed + 1);
        if (rv != EXB_OK) {
            return rv;
        }
        exb_assert_s(needed < str.cap, "str grow failed");
        va_copy(ap, ap_in);
        needed = vsnprintf(str.str, str.cap, fmt, ap);
        exb_assert_s(needed < str.cap, "str grow failed");
        va_end(ap);
    }
    str.len = needed;
    fprintf(stderr, "%s", str.str);
    exb_str_deinit(exb, &str);
    return EXB_OK;
}
static int exb_logger_logf(struct exb *exb, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int rv = exb_logger_logvf(exb,fmt, ap);
    va_end(ap);
    return rv;
}