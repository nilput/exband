#ifndef EXB_REQUEST_RULES_H
#define EXB_REQUEST_RULES_H
#include "../exb_errors.h"
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
};

struct exb_request_sink_filesystem {
    struct exb_str fs_path;
    int alias_len; //Initialized to 0, if alias:true is present in config
                   //then this gets patched to the length of the prefix in the parent config rule
};

struct exb_request_sink {
    enum exb_request_sink_type stype;
    union {
        struct exb_request_sink_filesystem fs;
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

static int exb_request_sink_filesystem_fixup(struct exb *exb_ref,
                                             struct exb_request_rule *rule,
                                             struct exb_request_sink *sink)
{
    struct exb_request_sink_filesystem *sink_fs = &sink->u.fs;
    if (sink->u.fs.alias_len) {
        //at this stage alias_len is used as a boolean
        if (rule->type != EXB_REQ_RULE_PATH_PREFIX) {
            exb_on_config_error(exb_ref, "Alias can only be used with prefix rules");
            return EXB_CONFIG_ERROR;
        }
        sink->u.fs.alias_len = rule->u.prefix_rule.prefix.len;
    }
    return EXB_OK;
}

/*Sink functions*/
/*Filesystem sink functions*/
//transfers ownership of path
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
    //This is only temporary, this gets fixed in fixup function
    sink_fs->alias_len = is_alias;

    return EXB_OK;
}
static int exb_request_sink_filesystem_deinit(struct exb *exb_ref,
                                          struct exb_request_sink_filesystem *sink_fs)
{
    exb_str_deinit(exb_ref, &sink_fs->fs_path);
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
    else if (sink->stype != EXB_REQ_SINK_NONE) {
        exb_assert_h(0, "invalid sink type");
    }
    sink->stype = EXB_REQ_SINK_NONE;
    return EXB_OK;
}

#endif
