#include <stdlib.h>
#ifndef CPB_ERRORS_H
#define CPB_ERRORS_H

#define CPBEF(error_name)
#define CPB_ERR_LIST \
    CPBEF(CPB_OK) \
    CPBEF(CPB_EOF) \
    CPBEF(CPB_SOCKET_ERR) \
    CPBEF(CPB_SELECT_ERR) \
    CPBEF(CPB_READ_ERR) \
    CPBEF(CPB_ACCEPT_ERR) \
    CPBEF(CPB_BIND_ERR) \
    CPBEF(CPB_LISTEN_ERR) \
    CPBEF(CPB_NOMEM_ERR) 
#undef CPBEF
#define CPBEF(error_name) error_name,
enum cpb_errors {
   CPB_ERR_LIST
};

#undef CPBEF
#define CPBEF(error_name) {error_name, #error_name,},
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
static struct cpb_error cpb_make_error(int error_code) {
    struct cpb_error err;
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