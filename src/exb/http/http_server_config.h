#ifndef EXB_HTTP_SERVER_CONFIG_H
#define EXB_HTTP_SERVER_CONFIG_H
#include "http_request_rules.h"
#include "../exb_log.h"
#include <stdbool.h>

#ifdef EXB_WITH_SSL
    #include "../mods/ssl/exb_ssl_config_entry.h"
#endif
//This will be treated as an opaque pointer, it will be null in case SSL is not used
struct exb_ssl_config_entry *ssl_config;

struct exb_http_domain_config {
    int http_listen_port; //0 means no HTTP listen
    int rules_begin; //an index in request_rules
    int rules_end;  //an exclusive end index in request_rules
    struct exb_str server_name;
    #ifdef EXB_WITH_SSL
        struct exb_ssl_config_entry ssl_config;
    #endif
};

struct exb_http_server_config {
    int z_http_listen_port;
    int http_use_aio;

    struct exb_request_rule request_rules[EXB_SERVER_MAX_RULES];
    int nrules;
    struct exb_request_sink request_sinks[EXB_SERVER_MAX_RULES];
    int nrulesinks;
    struct exb_str polling_backend;

    struct exb_http_domain_config domains[EXB_SNI_MAX_DOMAINS];
    int n_domains;

    struct {
        struct exb_str module_spec; //path:entry_name
        struct exb_str module_args;
    } module_specs[EXB_SERVER_MAX_MODULES];
    int n_modules;
};
static struct exb_http_server_config exb_http_server_config_default(struct exb *exb_ref) {
    (void) exb_ref;
    struct exb_http_server_config conf = {0};
    conf.n_domains = 0;
    conf.http_use_aio = 0;
    conf.n_modules = 0;
    conf.nrulesinks = 0;
    conf.nrules = 0;
    exb_str_init_const_str(&conf.polling_backend, "select");
    return conf;
}

/*
    a port with a value of 0 indicates no listen directive
    arguments are copied, no ownership is transferred
    server_name can be null
    ssl_config can be null, and must be null in case ssl is not supported
*/
static int exb_http_server_config_add_domain_ex(struct exb *exb_ref,
                                                struct exb_http_server_config *conf,
                                                int http_listen_port,
                                                int is_default,
                                                const char *server_name,
                                                struct exb_ssl_config_entry *ssl_config)
{
    if (conf->n_domains >= EXB_MAX_DOMAINS) {
        return EXB_OUT_OF_RANGE_ERR;
    }
    int rv = EXB_OK;
    struct exb_http_domain_config *domain = &conf->domains[conf->n_domains];
    if (server_name) {
        rv = exb_str_init_strcpy(exb_ref, &domain->server_name, server_name);
        if (rv != EXB_OK) {
            return rv;
        }
    }
    else {
        exb_str_init_empty(&domain->server_name);
    }
    
    #ifdef EXB_WITH_SSL
        exb_ssl_config_entry_init(exb_ref, &domain->ssl_config);
        if (ssl_config) {
            struct exb_ssl_config_entry *ssl_dest = &domain->ssl_config;
            rv = exb_str_copy(exb_ref, &ssl_dest->ca_path, &ssl_config->ca_path);
            if (rv == EXB_OK) {
                rv = exb_str_copy(exb_ref, &ssl_dest->private_key_path, &ssl_config->private_key_path);
            }
            if (rv == EXB_OK) {
                rv = exb_str_copy(exb_ref, &ssl_dest->public_key_path, &ssl_config->public_key_path);
            }
            if (rv == EXB_OK) {
                rv = exb_str_copy(exb_ref, &ssl_dest->dh_params_path, &ssl_config->dh_params_path);
            }
            if (rv == EXB_OK) {
                rv = exb_str_copy(exb_ref, &ssl_dest->ssl_ciphers, &ssl_config->ssl_ciphers);
            }
            if (rv == EXB_OK) {
                rv = exb_str_copy(exb_ref, &ssl_dest->ssl_protocols, &ssl_config->ssl_protocols);
            }
            if (rv == EXB_OK && server_name) {
                //This is just a copy, to be accessed by the SSL module
                rv = exb_str_strcpy(exb_ref, &ssl_dest->server_name, server_name);
            }
            if (rv != EXB_OK) {
                exb_str_deinit(exb_ref, &domain->server_name);
                exb_ssl_config_entry_deinit(exb_ref, ssl_dest);
                return rv;
            }
            ssl_dest->listen_port = ssl_config->listen_port;
            ssl_dest->is_default = ssl_config->is_default;
        }
    #else
        if (ssl_config) {
            exb_log_error(exb_ref, "SSL Config was provided but exband was not built with SSL support");
            return EXB_CONFIG_ERROR;
        }
    #endif
    domain->http_listen_port = http_listen_port;
    domain->rules_begin = conf->nrules;
    domain->rules_end   = conf->nrules;
    
    conf->n_domains++;
    return EXB_OK;
}

static int exb_http_server_config_add_domain(struct exb *exb_ref,
                                             struct exb_http_server_config *conf,
                                             int http_listen_port,
                                             int is_default)
{
    return exb_http_server_config_add_domain_ex(exb_ref, conf, http_listen_port, is_default, NULL, NULL);
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


/*Must be called before the sink is ready*/
/*This does possible optimizations or remaining initilization that depends on the parent rule*/
//Internal function
static int exb_http_server_config_request_rule_sink_fixup_(struct exb *exb_ref,
                                       struct exb_http_server_config *config,
                                       int rule_id,
                                       int sink_id) 
{
    struct exb_request_rule *rule = config->request_rules + rule_id;
    struct exb_request_sink *sink = config->request_sinks + sink_id;
    if (sink->stype == EXB_REQ_SINK_FILESYSTEM) {
        return exb_request_sink_filesystem_fixup(exb_ref, rule, sink);
    }
    return EXB_OK;
}

//transfers ownership
//currently this is stateful, the rules are added to the last added domain
static int exb_http_server_config_add_rule(struct exb *exb_ref,
                                           struct exb_http_server_config *config,
                                           struct exb_request_rule rule) 
{
    if (config->n_domains == 0) {
        return EXB_OUT_OF_RANGE_ERR;
    }
    if (config->nrules == EXB_SERVER_MAX_RULES) {
        return EXB_OUT_OF_RANGE_ERR;
    }
    config->request_rules[config->nrules] = rule;
    int rv = exb_http_server_config_request_rule_sink_fixup_(exb_ref, config, config->nrules, rule.sink_id);
    if (rv != EXB_OK) {
        return rv;
    }

    struct exb_http_domain_config *domain = config->domains + config->n_domains - 1;
    domain->rules_end++;
    config->nrules++;
    exb_assert_h(domain->rules_end == config->nrules, "");

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
