#ifndef EXB_HTTP_SERVER_CONFIG_H
#define EXB_HTTP_SERVER_CONFIG_H
#include "http_request_rules.h"
#include "../exb_str_list.h"
#include "../exb_log.h"
#include <stdbool.h>

#include "exb_ssl_config_entry.h"

//This will be treated as an opaque pointer, it will be null in case SSL is not used
struct exb_ssl_config_entry *ssl_config;

struct exb_http_domain_config {
    int http_listen_port; //0 means no HTTP listen
    struct exb_str http_listen_ip;
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
    int n_rules;
    struct exb_request_sink request_sinks[EXB_SERVER_MAX_RULES];
    int n_rule_sinks;
    struct exb_str polling_backend;

    struct exb_http_domain_config domains[EXB_SNI_MAX_DOMAINS];
    int n_domains;

    struct {
        struct exb_str module_spec; //path:entry_name
        struct exb_str module_args;
        struct exb_str_list import_list; //list of symbols to be imported from the library
    } module_specs[EXB_SERVER_MAX_MODULES];
    int n_modules;

    struct {
        struct exb_str handler_name; 
        int sink_id;
    } named_handlers[EXB_SERVER_MAX_NAMED_HANDLERS];
    int n_named_handlers;
};

static struct exb_http_server_config exb_http_server_config_default(struct exb *exb_ref) {
    (void) exb_ref;
    struct exb_http_server_config conf = {0};
    conf.n_domains = 0;
    conf.http_use_aio = 0;
    conf.n_modules = 0;
    conf.n_named_handlers = 0;
    conf.n_rule_sinks = 0;
    conf.n_rules = 0;
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
                                                struct exb_str *http_listen_ip,
                                                int is_default,
                                                const char *server_name,
                                                struct exb_ssl_config_entry *ssl_config)
{
    if (conf->n_domains >= EXB_MAX_DOMAINS) {
        return EXB_OUT_OF_RANGE_ERR;
    }
    int rv = EXB_OK;
    struct exb_http_domain_config *domain = &conf->domains[conf->n_domains];
    exb_str_init_empty(&domain->server_name);
    exb_str_init_empty(&domain->http_listen_ip);
    if (server_name) {
        rv = exb_str_init_strcpy(exb_ref, &domain->server_name, server_name);
        if (rv != EXB_OK) {
            return rv;
        }
    }
    if (http_listen_ip) {
        rv = exb_str_init_copy(exb_ref, &domain->http_listen_ip, http_listen_ip);
        if (rv != EXB_OK) {
            exb_str_deinit(exb_ref, &domain->server_name);
            return rv;
        }
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
            if (rv == EXB_OK) {
                rv = exb_str_copy(exb_ref, &ssl_dest->listen_ip, &ssl_config->listen_ip);
            }
            if (rv == EXB_OK && server_name) {
                //This is just a copy, to be accessed by the SSL module
                rv = exb_str_strcpy(exb_ref, &ssl_dest->server_name, server_name);
            }
            if (rv != EXB_OK) {
                exb_str_deinit(exb_ref, &domain->server_name);
                exb_str_deinit(exb_ref, &domain->http_listen_ip);
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
    domain->rules_begin = conf->n_rules;
    domain->rules_end   = conf->n_rules;
    
    conf->n_domains++;
    return EXB_OK;
}


/*handler_name is not owned*/
static int exb_http_server_config_lookup_handler(struct exb *exb_ref,
                                                struct exb_http_server_config *conf,
                                                char *handler_name,
                                                int *sink_id_out)
{
    for (int i=0; i < conf->n_named_handlers; i++) {
        if (exb_str_streqc(exb_ref, &conf->named_handlers[i].handler_name, handler_name)) {
            *sink_id_out = conf->named_handlers[i].sink_id;;
            return EXB_OK;
        }
    }
    return EXB_NOT_FOUND;
}

/*handler_name is not owned*/
static int exb_http_server_config_add_named_handler(struct exb *exb_ref,
                                                    struct exb_http_server_config *conf,
                                                    char *handler_name,
                                                    int sink_id)
{
    if (conf->n_named_handlers >= EXB_SERVER_MAX_NAMED_HANDLERS)
        return EXB_OUT_OF_RANGE_ERR;
    int rv = exb_str_init_strcpy(exb_ref, &conf->named_handlers[conf->n_named_handlers].handler_name, handler_name);
    if (rv != EXB_OK) {
        return rv;
    }
    conf->named_handlers[conf->n_named_handlers].sink_id = sink_id;
    conf->n_named_handlers++;
    return EXB_OK;
}



static int exb_http_server_config_add_domain(struct exb *exb_ref,
                                             struct exb_http_server_config *conf,
                                             int http_listen_port,
                                             int is_default)
{
    return exb_http_server_config_add_domain_ex(exb_ref, conf, http_listen_port, NULL, is_default, NULL, NULL);
}

static int exb_http_server_config_remove_rule(struct exb *exb_ref,
                                              struct exb_http_server_config *config,
                                              int rule_id);
static int exb_http_server_config_remove_sink(struct exb *exb_ref,
                                              struct exb_http_server_config *config,
                                              int sink_id);


static void exb_http_server_config_deinit(struct exb *exb_ref, struct exb_http_server_config *config) {
    for (int i=0; i < config->n_rules; i++) {
        exb_http_server_config_remove_rule(exb_ref, config, i);
    }
    for (int i=0; i < config->n_rule_sinks; i++) {
        exb_http_server_config_remove_sink(exb_ref, config, i);
    }
    for (int i=0; i < config->n_named_handlers; i++) {
        exb_str_deinit(exb_ref, &config->named_handlers[i].handler_name);
    }
    exb_assert_h(config->n_modules <= EXB_SERVER_MAX_MODULES, "");
    for (int i=0; i<config->n_modules; i++) {
        exb_str_deinit(exb_ref, &config->module_specs[i].module_spec);
        exb_str_deinit(exb_ref, &config->module_specs[i].module_args);
        exb_str_list_deinit(exb_ref, &config->module_specs[i].import_list);
    }
    exb_str_deinit(exb_ref, &config->polling_backend);
}




static int exb_http_server_config_rename_and_remove_sink(struct exb *exb_ref,
                                              struct exb_http_server_config *config,
                                              int sink_old_id,
                                              int sink_new_id);



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
    else if (sink->stype == EXB_REQ_SINK_FPTR) {
        return exb_request_sink_fptr_fixup(exb_ref, rule, sink);
    }
    else if (sink->stype == EXB_REQ_SINK_INTERMEDIATE_REF_TO_SINK) {
        exb_http_server_config_rename_and_remove_sink(exb_ref, config, sink_id, config->request_sinks[sink_id].u.reference_to_sink_id);
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
    if (config->n_rules == EXB_SERVER_MAX_RULES) {
        return EXB_OUT_OF_RANGE_ERR;
    }
    config->request_rules[config->n_rules] = rule;
    struct exb_http_domain_config *domain = config->domains + config->n_domains - 1;
    domain->rules_end++;
    config->n_rules++;

    int rv = exb_http_server_config_request_rule_sink_fixup_(exb_ref, config, config->n_rules, rule.sink_id);
    if (rv != EXB_OK) {
        domain->rules_end--;
        config->n_rules--;
        return rv;
    }
    exb_assert_h(domain->rules_end == config->n_rules, "");
    return EXB_OK;
}



//transfers ownership
static int exb_http_server_config_add_sink(struct exb *exb_ref,
                                           struct exb_http_server_config *config,
                                           struct exb_request_sink sink,
                                           int *sink_id_out)
{
    if (config->n_rules == EXB_SERVER_MAX_RULES) {
        return EXB_OUT_OF_RANGE_ERR;
    }
    config->request_sinks[config->n_rule_sinks] = sink;
    *sink_id_out = config->n_rule_sinks++;
    return EXB_OK;
}

/*handler_name is not owned*/
/*Adds an incomplete sink to be patched at runtime when the module is loaded*/
static int exb_http_server_config_add_named_empty_fptr_sink(struct exb *exb_ref,
                                                      struct exb_http_server_config *config,
                                                      char *handler_name,
                                                      int module_id)
{
    int rv = EXB_OK;
    struct exb_request_sink sink;
    rv = exb_request_sink_fptr_init(exb_ref,
                                    module_id,
                                    NULL,
                                    &sink);
    if (rv != EXB_OK) {
        return rv;
    }
    int sink_id = -1;
    rv = exb_http_server_config_add_sink(exb_ref,
                                         config,
                                         sink,
                                         &sink_id);
    if (rv != EXB_OK) {
        exb_request_sink_deinit(exb_ref, &sink);
        return rv;
    }
        
    rv = exb_http_server_config_add_named_handler(exb_ref,
                                                  config,
                                                  handler_name,
                                                  sink_id);
    if (rv != EXB_OK) {
        exb_http_server_config_remove_sink(exb_ref, config, sink_id);
        return rv;
    }
    return EXB_OK;
}

static void exb_http_server_config_rename_sink(struct exb *exb_ref,
                                              struct exb_http_server_config *config,
                                              int sink_old_id,
                                              int sink_new_id)
{
    for (int i = 0; i < config->n_rules; i++) {
        if (config->request_rules[i].sink_id == sink_old_id) {
            config->request_rules[i].sink_id = sink_new_id;
        }
    }
    for (int i = 0; i < config->n_named_handlers; i++) {
        if (config->named_handlers[i].sink_id == sink_old_id) {
            config->named_handlers[i].sink_id = sink_new_id;
        }
    }
}

static int exb_http_server_config_rename_and_remove_sink(struct exb *exb_ref,
                                              struct exb_http_server_config *config,
                                              int sink_old_id,
                                              int sink_new_id)
{
    /*
     * Step 1: rename all rule references to sink_old_id to a sentinel sink_id
     * Step 2: for all i = sink_id + 1 and greater:
     *      move sinks[i] to sinks[i - 1] to compact the array
     *      rename rule references to i to be i - 1
     *      if i is the sink_new_id then
     *          sink_new_id = i - 1
     * Step 3: rename all rule references to sentinel to point to target
     */
    int sentinel = config->n_rule_sinks + 1;
    int target = sink_new_id;
    exb_http_server_config_rename_sink(exb_ref, config, sink_old_id, sentinel);
    for (int i = sink_old_id + 1; i < config->n_rule_sinks; i++) {
        config->request_sinks[i - 1] = config->request_sinks[i];
        exb_http_server_config_rename_sink(exb_ref, config, i, i - 1);
        if (i == sink_new_id)
            target = sink_new_id - 1;
    }
    exb_http_server_config_rename_sink(exb_ref, config, sentinel, target);
    return EXB_OK;
}

static int exb_http_server_config_remove_sink(struct exb *exb_ref,
                                              struct exb_http_server_config *config,
                                              int sink_id)
{
    if (sink_id >= config->n_rule_sinks || sink_id < 0) {
        return EXB_OUT_OF_RANGE_ERR;
    }

    exb_request_sink_deinit(exb_ref, &config->request_sinks[sink_id]);
    for (int i = config->n_rule_sinks - 1;
        i >= 0 && config->request_sinks[i].stype == EXB_REQ_SINK_NONE;
        i--) 
    {
        config->n_rule_sinks--;
    }
    
    return EXB_OK;
}

static int exb_http_server_config_remove_rule(struct exb *exb_ref,
                                              struct exb_http_server_config *config,
                                              int rule_id)
{
    if (rule_id >= config->n_rules || rule_id < 0) {
        return EXB_OUT_OF_RANGE_ERR;
    }

    exb_request_rule_deinit(exb_ref, &config->request_rules[rule_id]);

    for (int i = config->n_rules - 1;
        i >= 0 && config->request_rules[i].type == EXB_REQ_RULE_NONE;
        i--) 
    {
        config->n_rules--;
    }
    
    return EXB_OK;
}


//checks whether aliases are all resolved
static int exb_http_server_config_check(struct exb *exb_ref, struct exb_http_server_config *config) {
    for (int i=0; i<config->n_rules; i++) {
        struct exb_request_rule *rule = &config->request_rules[i];
        //TODO
    }
    //TODO: check that all fptr handlers are not null, and that there are no duplicate names
    return EXB_OK;
}


#endif
