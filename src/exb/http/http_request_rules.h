#ifndef EXB_REQUEST_RULES_H
#define EXB_REQUEST_RULES_H
#include "../exb_errors.h"
#include "http_request_handler.h"
/*
abstractions used:
    rule: something to match requests on, for example a prefix path like "/files"
    sink: a destination for one or possibly more than one rule (for example a filesystem path like "/var/www/mysite/html/")

sink_id: the handler destination
*/
enum exb_request_rule_type {
    EXB_REQ_RULE_NONE,  //Represents a removed rule, DO NOT USE THIS AS A CATCH ALL
    EXB_REQ_RULE_PATH_PREFIX,
};
struct exb_request_path_prefix_rule {
    struct exb_str prefix;
};


struct exb_request_rule {
    enum exb_request_rule_type type;
    int sink_id;
    union {
        struct exb_request_path_prefix_rule prefix_rule;
    } u;
};

enum exb_request_sink_type {
    EXB_REQ_SINK_NONE, //Represents a removed sink, DO NOT USE THIS AS A CATCH ALL
    EXB_REQ_SINK_FILESYSTEM,
    EXB_REQ_SINK_FPTR,
    EXB_REQ_SINK_INTERMEDIATE_REF_TO_SINK, //this gets optimized out
};

struct exb_request_sink_filesystem {
    struct exb_str fs_path;
    int alias_len; //Initialized to 0, if alias:true is present in config
                   //then this gets patched to the length of the prefix in the parent config rule
};

struct exb_request_sink_fptr {
    exb_request_handler_func func;
    void *rqh_state;
    int module_id;
};

struct exb_request_sink {
    enum exb_request_sink_type stype;
    union {
        struct exb_request_sink_filesystem fs; /*EXB_REQ_SINK_FILESYSTEM*/
        struct exb_request_sink_fptr fptr; /*EXB_REQ_SINK_FPTR*/
        int reference_to_sink_id; /*EXB_REQ_SINK_INTERMEDIATE_REF_TO_SINK*/
    } u;
};

/*Rule functions*/
/*Prefix rule functions*/

//transfers ownership of prefix
static int exb_request_prefix_rule_init(struct exb *exb_ref,
                                        char *prefix,
                                        int sink_id,
                                        struct exb_request_rule *rule_out) 
{
    int rv;
    rule_out->type = EXB_REQ_RULE_PATH_PREFIX;
    rule_out->sink_id = sink_id;

    struct exb_request_path_prefix_rule *prule = &rule_out->u.prefix_rule;
    if ((rv = exb_str_init_transfer(exb_ref, prefix, &prule->prefix)) != EXB_OK) {
        return rv;
    }

    return EXB_OK;
}

static int exb_request_prefix_rule_deinit(struct exb *exb_ref,
                                         struct exb_request_path_prefix_rule *prule)
{
    exb_str_deinit(exb_ref, &prule->prefix);
    return EXB_OK;
}



static int exb_request_sink_intermediate_ref_to_sink_fixup(struct exb *exb_ref,
                                                                struct exb_request_rule *rule,
                                                                struct exb_request_sink *sink)
{
    EXB_UNUSED(exb_ref);
    EXB_UNUSED(rule);
    EXB_UNUSED(sink);
    return EXB_OK;
}

static int exb_request_sink_intermediate_ref_to_sink_deinit(struct exb *exb_ref,
                                        struct exb_request_sink_fptr *sink_fptr)
{
    return EXB_OK;
}

static int exb_request_sink_intermediate_ref_to_sink_init(struct exb *exb_ref,
                                                          int sink_id,
                                                          struct exb_request_sink  *sink_out)
{
    sink_out->stype = EXB_REQ_SINK_INTERMEDIATE_REF_TO_SINK;
    sink_out->u.reference_to_sink_id = sink_id;
    return EXB_OK;
}


static int exb_request_sink_fptr_deinit(struct exb *exb_ref,
                                        struct exb_request_sink_fptr *sink_fptr)
{
    return EXB_OK;
}

static int exb_request_sink_fptr_init(struct exb *exb_ref,
                                      int module_id,
                                      exb_request_handler_func func,
                                      struct exb_request_sink  *sink_out)
{
    sink_out->stype = EXB_REQ_SINK_FPTR;
    struct exb_request_sink_fptr *sink_fptr = &sink_out->u.fptr;
    sink_fptr->func = func;
    sink_fptr->module_id = module_id;
    return EXB_OK;
}

static int exb_request_sink_fptr_fixup(struct exb *exb_ref,
                                       struct exb_request_rule *rule,
                                       struct exb_request_sink *sink)
{
    EXB_UNUSED(exb_ref);
    EXB_UNUSED(rule);
    EXB_UNUSED(sink);
    return EXB_OK;
}


static int strlen_excluding_trailing_slash(char *str, int len) {
    while (str[len - 1] == '/')
        len--;
    return len;
}



/**
 * \internal
 * Initialize a filesystem handler request sink.
 * @param [in, owned] path - the path to a directory to serve requests from
 * @param is_alias - a boolean specifying whether the sink has alias enabled
 * @param sink_out - an uninitialized \struct exb_request_sink which the sink will be saved to
**/
static int exb_request_sink_filesystem_init(struct exb *exb_ref,
                                            char *path,
                                            int is_alias,
                                            struct exb_request_sink *sink_out)
{
    int rv;
    sink_out->stype = EXB_REQ_SINK_FILESYSTEM;
    
    struct exb_request_sink_filesystem *sink_fs = &sink_out->u.fs;
    if ((rv = exb_str_init_transfer(exb_ref, path, &sink_fs->fs_path)) != EXB_OK) {
        return rv;
    }
    
    sink_out->u.fs.fs_path.len = strlen_excluding_trailing_slash(sink_out->u.fs.fs_path.str, sink_out->u.fs.fs_path.len);
    sink_out->u.fs.fs_path.str[sink_out->u.fs.fs_path.len] = 0;
    
    //This is only temporary, this gets fixed in fixup function, currently only storing a boolean value
    sink_fs->alias_len = is_alias;
    //ugh, this could be refactored so that this information is available at this call

    return EXB_OK;
}
/**
 * \internal
 * Destroy a request sink.
**/
static int exb_request_sink_filesystem_deinit(struct exb *exb_ref,
                                          struct exb_request_sink_filesystem *sink_fs)
{
    exb_str_deinit(exb_ref, &sink_fs->fs_path);
    return EXB_OK;
}

static int exb_request_sink_filesystem_fixup(struct exb *exb_ref,
                                             struct exb_request_rule *rule,
                                             struct exb_request_sink *sink)
{
    struct exb_request_sink_filesystem *sink_fs = &sink->u.fs;
    if (sink->u.fs.alias_len) {
        struct exb_str *prefix = &rule->u.prefix_rule.prefix;
        // at this stage alias_len is used as a boolean
        if (rule->type != EXB_REQ_RULE_PATH_PREFIX) {
            exb_on_config_error(exb_ref, "Alias can only be used with prefix rules");
            return EXB_CONFIG_ERROR;
        }
        if (prefix->len < 1                  ||
            prefix->str[0] != '/'            || 
            prefix->str[prefix->len - 1] != '/')
        {
            exb_on_config_error(exb_ref, "Prefix rule must have a trailing slash when alias is enabled");
            return EXB_CONFIG_ERROR;
        }
        // prefix itself is assumed to contain a leading and trailing slash (done at rule initialization)
        sink->u.fs.alias_len = strlen_excluding_trailing_slash(prefix->str, prefix->len);
    }
    return EXB_OK;
}



static int exb_request_rule_deinit(struct exb *exb_ref,
                                   struct exb_request_rule *rule)
{
    if (rule->type == EXB_REQ_RULE_PATH_PREFIX) {
        exb_request_prefix_rule_deinit(exb_ref, &rule->u.prefix_rule);
    }
    else if (rule->type != EXB_REQ_RULE_NONE) {
       exb_assert_h(0, "invalid rule type");
    }
    rule->type = EXB_REQ_RULE_NONE;
    return EXB_OK;
}

static int exb_request_sink_deinit(struct exb *exb_ref,
                                   struct exb_request_sink *sink)
{
    if (sink->stype == EXB_REQ_SINK_FILESYSTEM) {
        exb_request_sink_filesystem_deinit(exb_ref, &sink->u.fs);
    }
    else if (sink->stype == EXB_REQ_SINK_FPTR) {
        exb_request_sink_fptr_deinit(exb_ref, &sink->u.fptr);
    }
    else if (sink->stype != EXB_REQ_SINK_NONE) {
        exb_assert_h(0, "invalid sink type");
    }
    sink->stype = EXB_REQ_SINK_NONE;
    return EXB_OK;
}

#endif
