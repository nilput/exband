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
    struct exb_request_sink request_sinks[EXB_SERVER_MAX_RULES];
    int nrulesinks;
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

static int exb_http_server_config_remove_rule(struct exb *exb_ref,
                                              struct exb_http_server_config *config,
                                              int rule_id);
static int exb_http_server_config_remove_sink(struct exb *exb_ref,
                                              struct exb_http_server_config *config,
                                              int sink_id);


static void exb_http_server_config_deinit(struct exb *exb_ref, struct exb_http_server_config *config) {
    for (int i=0; i < config->nrules; i++) {
        exb_http_server_config_remove_rule(exb_ref, config, i);
    }
    for (int i=0; i < config->nrulesinks; i++) {
        exb_http_server_config_remove_sink(exb_ref, config, i);
    }
    exb_assert_h(config->n_modules <= EXB_SERVER_MAX_MODULES, "");
    for (int i=0; i<config->n_modules; i++) {
        exb_str_deinit(exb_ref, &config->module_specs[i].module_spec);
        exb_str_deinit(exb_ref, &config->module_specs[i].module_args);
    }
    exb_str_deinit(exb_ref, &config->polling_backend);
    
}


//transfers ownership
static int exb_http_server_config_add_rule(struct exb *exb_ref,
                                           struct exb_http_server_config *config,
                                           struct exb_request_rule rule) 
{
    if (config->nrules == EXB_SERVER_MAX_RULES) {
        return EXB_OUT_OF_RANGE_ERR;
    }
    config->request_rules[config->nrules] = rule;
    config->nrules++;
    return EXB_OK;
}

//transfers ownership
static int exb_http_server_config_add_sink(struct exb *exb_ref,
                                           struct exb_http_server_config *config,
                                           struct exb_request_sink sink,
                                           int *sink_id_out)
{
    if (config->nrules == EXB_SERVER_MAX_RULES) {
        return EXB_OUT_OF_RANGE_ERR;
    }
    config->request_sinks[config->nrulesinks] = sink;
    *sink_id_out = config->nrulesinks++;
    return EXB_OK;
}
static int exb_http_server_config_remove_sink(struct exb *exb_ref,
                                              struct exb_http_server_config *config,
                                              int sink_id)
{
    if (sink_id >= config->nrulesinks || sink_id < 0) {
        return EXB_OUT_OF_RANGE_ERR;
    }

    exb_request_sink_deinit(exb_ref, &config->request_sinks[sink_id]);
    for (int i = config->nrulesinks - 1;
        i >= 0 && config->request_sinks[i].stype == EXB_REQ_SINK_NONE;
        i--) 
    {
        config->nrulesinks--;
    }
    
    return EXB_OK;
}

static int exb_http_server_config_remove_rule(struct exb *exb_ref,
                                              struct exb_http_server_config *config,
                                              int rule_id)
{
    if (rule_id >= config->nrules || rule_id < 0) {
        return EXB_OUT_OF_RANGE_ERR;
    }

    exb_request_rule_deinit(exb_ref, &config->request_rules[rule_id]);
    for (int i = config->nrules - 1;
        i >= 0 && config->request_rules[i].type == EXB_REQ_RULE_NONE;
        i--) 
    {
        config->nrules--;
    }
    
    return EXB_OK;
}


//checks whether aliases are all resolved
static int exb_http_server_config_check(struct exb *exb_ref, struct exb_http_server_config *config) {
    for (int i=0; i<config->nrules; i++) {
        struct exb_request_rule *rule = &config->request_rules[i];
        //TODO
    }
    return EXB_OK;
}
#endif