#include <stdlib.h>
#include <stdio.h>
#ifndef EXB_ERRORS_H
#define EXB_ERRORS_H

#define CPERR(error_name)
#define EXB_ERR_LIST \
    CPERR(EXB_OK) \
    CPERR(EXB_EOF) \
    CPERR(EXB_CHILD) \
    CPERR(EXB_NOT_FOUND) \
    CPERR(EXB_INIT_ERROR) \
    CPERR(EXB_MODULE_LOAD_ERROR) \
    CPERR(EXB_SOCKET_ERR) \
    CPERR(EXB_SELECT_ERR) \
    CPERR(EXB_READ_ERR) \
    CPERR(EXB_WRITE_ERR) \
    CPERR(EXB_ACCEPT_ERR) \
    CPERR(EXB_BIND_ERR) \
    CPERR(EXB_OUT_OF_RANGE_ERR) \
    CPERR(EXB_BUFFER_FULL_ERR) \
    CPERR(EXB_INVALID_STATE_ERR) \
    CPERR(EXB_INVALID_INT_ERR) \
    CPERR(EXB_LISTEN_ERR) \
    CPERR(EXB_HTTP_ERROR) \
    CPERR(EXB_INVALID_ARG_ERR) \
    CPERR(EXB_UNSUPPORTED) \
    CPERR(EXB_MUTEX_LOCK_ERROR) \
    CPERR(EXB_EPOLL_INIT_ERROR) \
    CPERR(EXB_EPOLL_ADD_ERROR) \
    CPERR(EXB_EPOLL_WAIT_ERROR) \
    CPERR(EXB_FORK_ERROR) \
    CPERR(EXB_MUTEX_ERROR) \
    CPERR(EXB_THREAD_ERROR) \
    CPERR(EXB_CONFIG_ERROR) \
    CPERR(EXB_NOMEM_ERR) 
#undef CPERR
#define CPERR(error_name) error_name,
enum exb_errors {
   EXB_ERR_LIST
};

#undef CPERR
#define CPERR(error_name) {error_name, #error_name,},
struct exb_error_str {
    int error_code;
    const char *error_name;
};
static struct exb_error_str exb_error_str_list[] = {
    EXB_ERR_LIST
};
#define EXB_ERROR_LIST_COUNT (sizeof(exb_error_str_list)/sizeof(struct exb_error_str))

struct exb_error {
    int error_code;
    void *details;
};

static const char *exb_error_code_name(int error_code) {
    for (int i=0; i<EXB_ERROR_LIST_COUNT; i++) {
        if (exb_error_str_list[i].error_code == error_code) {
            return exb_error_str_list[i].error_name;
        }
    }
    return NULL;
}
static inline void exb_error_print(int error_code) {
    fprintf(stderr, "An Error occured: %s\n", exb_error_code_name(error_code));
}
static inline void exb_error_debug(int error_code) {
    #ifdef EXB_DEBUG
        exb_error_print(error_code);
    #endif
}
static inline struct exb_error exb_make_error(int error_code) {
    struct exb_error err;
    if (error_code != EXB_OK) 
        exb_error_debug(error_code);
    err.details = NULL;
    err.error_code = error_code;
    return err;
}
static struct exb_error exb_prop_error(struct exb_error src) {
    return src;
}

#endif
