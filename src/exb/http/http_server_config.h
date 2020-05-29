#ifndef EXB_HTTP_SERVER_CONFIG_H
#define EXB_HTTP_SERVER_CONFIG_H
#include "http_request_rules.h"
#include <stdbool.h>
struct exb_http_server_config {
    int http_listen_port;
    int http_use_aio;
    struct {
        struct exb_str module_spec; //path:entry_name
        struct exb_str module_args;
    } module_specs[EXB_SERVER_MAX_MODULES];
    
    int n_modules;
    struct exb_request_rule request_rules[EXB_SERVER_MAX_RULES];
    int nrules;
    struct exb_str polling_backend;
};
static struct exb_http_server_config exb_http_server_config_default(struct exb *exb_ref) {
    (void) exb_ref;
    struct exb_http_server_config conf = {0};
    conf.http_listen_port = 80;
    conf.http_use_aio = 0;
    conf.n_modules = 0;
    exb_str_init_const_str(&conf.polling_backend, "select");
    return conf;
}
static void exb_http_server_config_deinit(struct exb *exb_ref, struct exb_http_server_config *config) {
    exb_assert_h(config->n_modules <= EXB_SERVER_MAX_MODULES, "");
    for (int i=0; i<config->n_modules; i++) {
        exb_str_deinit(exb_ref, &config->module_specs[i].module_spec);
        exb_str_deinit(exb_ref, &config->module_specs[i].module_args);
    }
    exb_str_deinit(exb_ref, &config->polling_backend);    
    for (int i=0; i < config->nrules; i++) {
        exb_free(exb_ref, config->request_rules[i].u.prefix_rule.dest_alias);
        exb_free(exb_ref, config->request_rules[i].u.prefix_rule.prefix);
    }
}

//doesn't own path,alias parameters
static int exb_http_server_config_add_http_prefix_rule(struct exb *exb_ref, struct exb_http_server_config *config, const char *prefix, const char *dest_alias) {
    if (config->nrules == EXB_SERVER_MAX_RULES) {
        return EXB_OUT_OF_RANGE_ERR;
    }
    struct exb_str path_str;
    struct exb_str alias_str;
    int rv = EXB_OK;
    if ((rv = exb_str_init_strcpy(exb_ref, &path_str, prefix)) != EXB_OK) {
        return rv;
    }
    if ((rv = exb_str_init_strcpy(exb_ref, &alias_str, dest_alias)) != EXB_OK) {
        exb_str_deinit(exb_ref, &path_str);
        return rv;
    }
    config->request_rules[config->nrules].type = EXB_RRULE_PATH;
    config->request_rules[config->nrules].u.prefix_rule.dest_alias = alias_str.str;
    config->request_rules[config->nrules].u.prefix_rule.prefix = path_str.str;
    config->nrules++;

    return EXB_OK;
}

//boolean
static int exb_http_server_config_is_alias_defined(struct exb *exb_ref, struct exb_http_server_config *config, const char *alias) {
    return true;
}

//checks whether aliases are all resolved
static int exb_http_server_config_check(struct exb *exb_ref, struct exb_http_server_config *config) {
    for (int i=0; i<config->nrules; i++) {
        struct exb_request_rule *rule = &config->request_rules[i];
        if (rule->type == EXB_RRULE_PATH) {
            if (!exb_http_server_config_is_alias_defined(exb_ref, config, rule->u.prefix_rule.dest_alias)) {
                
                return EXB_CONFIG_ERROR;
            }
        }
    }
    return EXB_OK;
}
#endif