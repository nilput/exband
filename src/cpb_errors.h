#include <stdlib.h>
#ifndef CPB_ERRORS_H
#define CPB_ERRORS_H

#define CPERR(error_name)
#define CPB_ERR_LIST \
    CPERR(CPB_OK) \
    CPERR(CPB_EOF) \
    CPERR(CPB_SOCKET_ERR) \
    CPERR(CPB_SELECT_ERR) \
    CPERR(CPB_READ_ERR) \
    CPERR(CPB_WRITE_ERR) \
    CPERR(CPB_ACCEPT_ERR) \
    CPERR(CPB_BIND_ERR) \
    CPERR(CPB_OUT_OF_RANGE_ERR) \
    CPERR(CPB_BUFFER_FULL_ERR) \
    CPERR(CPB_INVALID_STATE_ERR) \
    CPERR(CPB_INVALID_INT_ERR) \
    CPERR(CPB_LISTEN_ERR) \
    CPERR(CPB_HTTP_ERROR) \
    CPERR(CPB_INVALID_ARG_ERR) \
    CPERR(CPB_UNSUPPORTED) \
    CPERR(CPB_MUTEX_LOCK_ERROR) \
    CPERR(CPB_EPOLL_INIT_ERROR) \
    CPERR(CPB_EPOLL_ADD_ERROR) \
    CPERR(CPB_EPOLL_WAIT_ERROR) \
    CPERR(CPB_MUTEX_ERROR) \
    CPERR(CPB_THREAD_ERROR) \
    CPERR(CPB_NOMEM_ERR) 
#undef CPERR
#define CPERR(error_name) error_name,
enum cpb_errors {
   CPB_ERR_LIST
};

#undef CPERR
#define CPERR(error_name) {error_name, #error_name,},
struct cpb_error_str {
    int error_code;
    const char *error_name;
};
static struct cpb_error_str cpb_error_str_list[] = {
    CPB_ERR_LIST
};
#define CPB_ERROR_LIST_COUNT (sizeof(cpb_error_str_list)/sizeof(struct cpb_error_str))

struct cpb_error {
    int error_code;
    void *details;
};
static void cpb_error_debug() {
    return;
}
static struct cpb_error cpb_make_error(int error_code) {
    struct cpb_error err;
    if (error_code != CPB_OK) cpb_error_debug();

    err.details = NULL;
    err.error_code = error_code;
    return err;
}
static struct cpb_error cpb_prop_error(struct cpb_error src) {
    return src;
}
static const char *cpb_error_name(struct cpb_error *err) {
    for (int i=0; i<CPB_ERROR_LIST_COUNT; i++) {
        if (cpb_error_str_list[i].error_code == err->error_code) {
            return cpb_error_str_list[i].error_name;
        }
    }
    return NULL;
}

#define define_cpb_or(value_type, struct_name) \
struct_name { \
    struct cpb_error error; \
    value_type value; \
}
#endif
