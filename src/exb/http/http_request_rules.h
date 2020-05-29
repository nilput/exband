#ifndef EXB_REQUEST_RULES_H
#define EXB_REQUEST_RULES_H
enum exb_request_rule_type {
    EXB_RRULE_PATH,
};
struct exb_request_path_prefix_rule {
    char *prefix;
    char *dest_alias; //can be null
};
struct exb_request_rule {
    enum exb_request_rule_type type;
    union {
        struct exb_request_path_prefix_rule prefix_rule;
    } u;
};
#endif