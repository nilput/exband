#include "exb.h"
#include "exb_str.h"
#include "exb_log.h"
#include "exb_utils.h"
#include "exb_build_config.h"
#include "http/http_server_config.h"
#include "util/varg.h" //argv parsing
#include "jsmn/jsmn.h"

//example path: foo.bar
/*
    {
        "foo" : {
            "bar" : 23
        }
    }
*/
int tokcount(jsmntok_t *t) {
    if (t->type == JSMN_PRIMITIVE) {
        return 1;
    } else if (t->type == JSMN_STRING) {
        return 1;
    } else if (t->type == JSMN_OBJECT) {
        int j = 1;
        for (int i = 0; i < t->size; i++) {
            jsmntok_t *key = t + j;
            j += tokcount(key);
            if (key->size > 0) {
                j += tokcount(t + j);
            }
        }
        return j;
    } else if (t->type == JSMN_ARRAY) {
        int j = 1;
        for (int i = 0; i < t->size; i++) {
            j += tokcount(t + j);
        }
        return j;
    }
    return 0;
}
jsmntok_t *json_get(char *json_str, jsmntok_t *t, const char *path) {
    if (strcmp(path, "") == 0)
        return t;
    if (t->type != JSMN_OBJECT) {
        return NULL;
    }
    char keyname[64];
    const char *next = strchr(path, '.');
    if (!next)
        next = path + strlen(path);
    
    if ((next - path + 1) > sizeof keyname)
        return NULL;
    strncpy(keyname, path, next - path);
    keyname[next - path] = 0;
    if (t->type == JSMN_OBJECT) {
        int j = 1;
        for (int i = 0; i < t->size; i++) {
            jsmntok_t *key = t + j;
            jsmntok_t *val = NULL;
            j += tokcount(key);
            if (key->size > 0) {
                if ( key->type == JSMN_STRING                      &&
                    (key->end - key->start) == (next - path)       &&
                    memcmp(key->start + json_str, keyname, next - path) == 0)
                {
                    if (strcmp(next, "") == 0)
                        return t + j;
                    return json_get(json_str, t + j, next[0] == '.' ? next + 1 : next);
                }
                j += tokcount(t + j);
            }
        }
    }
    return NULL;
}
int json_tok_to_buff(char *dest, int dest_sz, const char *json_str, jsmntok_t *tok) {
    if (dest_sz <= (tok->end - tok->start))
        return EXB_OUT_OF_RANGE_ERR;
    strncpy(dest, json_str + tok->start, tok->end -tok->start);
    dest[tok->end - tok->start] = 0;
    return EXB_OK;
}
int json_get_as_integer(char *json_str, jsmntok_t *t, int *out) {
    char buff[64];
    *out = 0;
    if (json_tok_to_buff(buff, sizeof buff, json_str, t) != EXB_OK)
        return EXB_OUT_OF_RANGE_ERR;
    if (t->type == JSMN_PRIMITIVE || t->type == JSMN_STRING) {
        *out = atoi(buff);
        return EXB_OK;
    }
    return EXB_INVALID_ARG_ERR;
}

int json_get_as_string_copy(struct exb *exb_ref, char *json_str, jsmntok_t *t, int do_coerce, char **out) {
    *out = NULL;
    if (t->type != JSMN_STRING && (!do_coerce || t->type != JSMN_PRIMITIVE)) {
        return EXB_INVALID_ARG_ERR;
    }
    char *s = exb_malloc(exb_ref, t->end - t->start + 1);
    if (!s) {
        return EXB_NOMEM_ERR;
    }
    *out = s;
    return json_tok_to_buff(s, t->end - t->start + 1, json_str, t);
}

//Assumes str_out is uninitialized
int json_get_as_string_copy_1(struct exb *exb_ref, char *json_str, jsmntok_t *t, int do_coerce, struct exb_str *str_out) {
    char *s = NULL;
    int rv = EXB_OK;
    if ((rv = json_get_as_string_copy(exb_ref, json_str, t, do_coerce, &s)) != EXB_OK)
        return rv;
    rv = exb_str_init_transfer(exb_ref, s, str_out);
    if (rv != EXB_OK) {
        exb_free(exb_ref, s);
        return rv;
    }
    return EXB_OK;
}
//return value like strcmp
int json_token_strcmp(char *json_str, jsmntok_t *t, const char *str) {
    if (strlen(str) != (t->end - t->start)) {
        return json_str[strlen(str)];
    }
    return strncmp(json_str + t->start, str, strlen(str));
}
int json_get_as_boolean(char *json_str, jsmntok_t *t, int *out) {
    *out = 0;
    if (t->type == JSMN_PRIMITIVE || t->type == JSMN_STRING) {
        if ((t->end - t->start) == 4 && strncmp("true", json_str + t->start, 4) == 0) {
            *out = 1;
            return EXB_OK;
        }
        else if ((t->end - t->start) == 5 && strncmp("false", json_str + t->start, 5) == 0) {
            *out = 0;
            return EXB_OK;
        }
    }
    return EXB_INVALID_ARG_ERR;
}

/*
valid input examples:
port like:        "80"
ip and port like: "127.0.0.1:80"
ip_out is expected to be uninitialized, it will hold a copy, the caller must free it after use
*/

int split_ip_port_pair(struct exb *exb_ref, char *ip_port, int *port_out, struct exb_str *ip_out)
{
    exb_str_init_empty(ip_out);
    char *colon = strchr(ip_port, ':');
    while (colon && strchr(colon + 1, ':'))
        colon = strchr(colon + 1, ':');
    if (colon) {
        if ((colon - ip_port) > 0) {
            int rv = exb_str_strlcpy(exb_ref, ip_out, ip_port, colon - ip_port);
            if (rv != EXB_OK)
                return rv;
        }
        *port_out = atoi(colon + 1);
    }
    else {
        *port_out = atoi(ip_port);
    }
    if (!*port_out) {
        return EXB_INVALID_FORMAT;
    }
    return EXB_OK;
}

int dump(char *json_str, jsmntok_t *t, int count, int indent) {
    int i, j, k;
    jsmntok_t *key;
    if (count == 0) {
        return 0;
    }
    if (t->type == JSMN_PRIMITIVE) {
        printf("%.*s", t->end - t->start, json_str + t->start);
        return 1;
    } else if (t->type == JSMN_STRING) {
        printf("'%.*s'", t->end - t->start, json_str + t->start);
        return 1;
    } else if (t->type == JSMN_OBJECT) {
        printf("\n");
        j = 0;
        for (i = 0; i < t->size; i++) {
            for (k = 0; k < indent; k++) {
                printf("  ");
            }
            key = t + 1 + j;
            j += dump(json_str, key, count - j, indent + 1);
            if (key->size > 0) {
                printf(": ");
                j += dump(json_str, t + 1 + j, count - j, indent + 1);
            }
            printf("\n");
        }
        return j + 1;
    } else if (t->type == JSMN_ARRAY) {
        j = 0;
        printf("\n");
        for (i = 0; i < t->size; i++) {
        for (k = 0; k < indent - 1; k++) {
            printf("  ");
        }
        printf("   - ");
        j += dump(json_str, t + 1 + j, count - j, indent + 1);
        printf("\n");
        }
        return j + 1;
    }
    return 0;

}
//transforms {"key" : "value", "foo":"bar"} to "key=value foo=bar"
static int json_to_key_eq_value_new(struct exb *exb_ref, const char *json_str, jsmntok_t *obj, char **out) {
    struct exb_str str;
    *out = NULL;
    if (!obj || (obj->type != JSMN_OBJECT))
        return EXB_INVALID_ARG_ERR;
    
    exb_str_init_empty(&str);
    int rv = EXB_OK;
    for (int i=0; i<obj->size; i++) {
        jsmntok_t *key =   obj + i * 2 + 1;
        jsmntok_t *value = obj + i * 2 + 2;
        if (key->size != 1) 
        {
            rv = EXB_INVALID_ARG_ERR;
            goto err;
        }
        
        if ((i != 0 && ((rv = exb_str_strlappend(exb_ref, &str, " ",   1))                          != EXB_OK))   ||
            (rv = exb_str_strlappend(exb_ref, &str, json_str + key->start,   key->end   - key->start))    != EXB_OK     ||
            (rv = exb_str_strlappend(exb_ref, &str, "=",               1))                          != EXB_OK     ||
            (rv = exb_str_strlappend(exb_ref, &str, json_str + value->start, value->end - value->start))  != EXB_OK)
        {
            goto err;
        }
    }
    *out = str.str;
    return EXB_OK;
err:
    exb_str_deinit(exb_ref, &str);
    return rv;
}

static int on_config_error(char *config_path, int lineno, char *error_msg) {
    fprintf(stderr, "Config Error: %s:%d %s", config_path, lineno, error_msg);
    return EXB_CONFIG_ERROR;
}



struct exb_json_parser_state {
    jsmn_parser parser;
    char *full_file;
    size_t file_size;
    jsmntok_t *tokens;
    int ntokens;
};



/*
initializes a request rule's sink from a json object
*/
static int parse_rule_destination(struct exb *exb_ref,
                                  struct exb_json_parser_state *ep,
                                  struct exb_http_server_config *http_server_config_in,
                                  jsmntok_t *obj,
                                  struct exb_request_sink *sink_out)
{
    jsmntok_t *dtype = json_get(ep->full_file, obj, "type");
    int rv = EXB_OK;
    if (dtype && json_token_strcmp(ep->full_file, dtype, "filesystem") == 0) {
        char *path_str = NULL;
        int is_alias = 0;
        jsmntok_t *path = json_get(ep->full_file, obj, "path");
        if (!path || json_get_as_string_copy(exb_ref, ep->full_file, path, 0, &path_str) != EXB_OK) {
            return EXB_CONFIG_ERROR;
        }
        jsmntok_t *alias = json_get(ep->full_file, obj, "alias");
        if (!alias || json_get_as_boolean(ep->full_file, alias, &is_alias) != EXB_OK) {
            is_alias = 0;
        }
        rv = exb_request_sink_filesystem_init(exb_ref, path_str, is_alias, sink_out);
        if (rv != EXB_OK) {
            exb_free(exb_ref, path_str);
            return rv;
        }
    }
    else if (dtype && json_token_strcmp(ep->full_file, dtype, "handler") == 0) {
        char *handler_name_str = NULL;
        jsmntok_t *handler_name = json_get(ep->full_file, obj, "name");
        if (!handler_name || json_get_as_string_copy(exb_ref, ep->full_file, handler_name, 0, &handler_name_str) != EXB_OK) {
            return EXB_CONFIG_ERROR;
        }
        int reference_sink_id;
        rv = exb_http_server_config_lookup_handler(exb_ref, http_server_config_in, handler_name_str, &reference_sink_id);
        if (rv != EXB_OK) {
            exb_log_error(exb_ref, "Handler '%s' was not found\n", handler_name_str);
            exb_free(exb_ref, handler_name_str);
            return EXB_CONFIG_ERROR;
        }
        exb_free(exb_ref, handler_name_str);
        handler_name_str = NULL;

        rv = exb_request_sink_intermediate_ref_to_sink_init(exb_ref, reference_sink_id, sink_out);
        if (rv != EXB_OK) {
            exb_free(exb_ref, handler_name_str);
            return EXB_CONFIG_ERROR;
        }
    }
    else {
        return EXB_CONFIG_ERROR;
    }
    //takes ownership of path_str
    return EXB_OK;

}

/*
initializes a request rule from a json object
*/
static int parse_rule(struct exb *exb_ref,
                      struct exb_json_parser_state *ep,
                      struct exb_http_server_config *http_server_config_in,
                      jsmntok_t *obj,
                      int sink_id,
                      struct exb_request_rule *rule_out)
{
    char *prefix_str = NULL;
    jsmntok_t *prefix = json_get(ep->full_file, obj, "prefix");
    if (!prefix ||
        json_get_as_string_copy(exb_ref, ep->full_file, prefix, 0, &prefix_str) != EXB_OK
       ) 
    {
        return EXB_CONFIG_ERROR;
    }
    int rv = exb_request_prefix_rule_init(exb_ref, prefix_str, sink_id, rule_out);
    if (rv != EXB_OK) {
        exb_free(exb_ref, prefix_str);
        return rv;
    }
    return EXB_OK;
}

static int load_json_rules(struct exb *exb_ref,
                           struct exb_json_parser_state *ep,
                           jsmntok_t *parent,
                           struct exb_http_server_config *http_server_config_out);
static int load_json_modules(struct exb *exb_ref,
                             struct exb_json_parser_state *ep,
                             struct exb_config *config_out,
                             struct exb_http_server_config *http_server_config_out);
static int load_json_servers(struct exb *exb_ref,
                             struct exb_json_parser_state *ep,
                             jsmntok_t *parent,
                             struct exb_http_server_config *http_server_config_out);
static int load_json_server(struct exb *exb_ref,
                             struct exb_json_parser_state *ep,
                             jsmntok_t *domain_obj,
                             struct exb_http_server_config *http_server_config_out);


/*in case of failure, currently the only good course of action is to destroy exb and start it again*/
static int load_json_config(const char *config_path, struct exb *exb_ref, struct exb_config *config_out, struct exb_http_server_config *http_server_config_out) {
    FILE *f = fopen(config_path, "r");
    if (!f) {
        return EXB_NOT_FOUND;
    }
    struct exb_json_parser_state ep = {0};
    jsmn_init(&ep.parser);
    if (exb_read_file_fully(exb_ref, f, 1024 * 128, &ep.full_file, &ep.file_size) != EXB_OK) {
        fclose(f);
        return EXB_NOT_FOUND;
    }
    fclose(f);
    f = NULL;

    int rv = JSMN_ERROR_NOMEM;
    while (rv == JSMN_ERROR_NOMEM) {
        ep.ntokens = ep.ntokens == 0 ? 8 : ep.ntokens * 2;
        ep.tokens = exb_realloc_f(exb_ref, ep.tokens, sizeof(jsmntok_t) * ep.ntokens);
        if (!ep.tokens) {
            return EXB_NOMEM_ERR;
        }
        rv = jsmn_parse(&ep.parser, ep.full_file, ep.file_size, ep.tokens, ep.ntokens);
    }
    if (rv < 0) {
        fprintf(stderr, "json parse failed: %s\n", config_path);
        rv = EXB_INVALID_ARG_ERR;
        goto on_error_1;
    }
    
    #ifdef EXB_DUMP_CONFIG
        dump(full_file, ep.tokens, parser.toknext, 0);
    #endif

    int integer;
    char *string;
    jsmntok_t *obj = NULL;
    obj = json_get(ep.full_file, ep.tokens, "event.threadpool");
    if (obj && json_get_as_integer(ep.full_file, obj, &integer) == 0) {
        config_out->tp_threads = integer;
    }
    obj = json_get(ep.full_file, ep.tokens, "event.loops");
    if (obj && json_get_as_integer(ep.full_file, obj, &integer) == 0) {
        config_out->nloops = integer;
    }
    obj = json_get(ep.full_file, ep.tokens, "event.mode");
    if (obj && json_get_as_string_copy(exb_ref, ep.full_file, obj, 0, &string) == 0) {
        if (exb_strcase_eq(string, "multithreading")) {
            config_out->op_mode = EXB_MODE_MULTITHREADING;
        }
        else if (exb_strcase_eq(string, "multiprocessing")) {
            config_out->op_mode = EXB_MODE_MULTIPROCESSING;
        }
        else {
            exb_free(exb_ref, string);
            return EXB_CONFIG_ERROR;
        }
        exb_free(exb_ref, string);
        string = NULL;
    }
    obj = json_get(ep.full_file, ep.tokens, "event.processes");
    if (obj && json_get_as_integer(ep.full_file, obj, &integer) == 0) {
        if (integer < 1) {
            integer = 1;
        }
        config_out->nprocess = integer;
    }
    obj = json_get(ep.full_file, ep.tokens, "event.polling");
    if (obj && json_get_as_string_copy(exb_ref, ep.full_file, obj, 0, &string) == 0) {
        if (exb_str_init_strcpy(exb_ref, &http_server_config_out->polling_backend, string) != EXB_OK) {
            rv = EXB_NOMEM_ERR;
            goto on_error_1;
        }
        exb_free(exb_ref, string);
        string = NULL;
    }
    obj = json_get(ep.full_file, ep.tokens, "event.aio");
    if (obj && (json_get_as_boolean(ep.full_file, obj, &integer) == 0)) {
        http_server_config_out->http_use_aio = !!integer;
    }
    rv = load_json_modules(exb_ref, &ep, config_out, http_server_config_out);
    if (rv != EXB_OK) {
            goto on_error_1;
    }
    obj = json_get(ep.full_file, ep.tokens, "http.listen");
    if (obj && (obj = json_get(ep.full_file, ep.tokens, "http"))) {
        rv = load_json_server(exb_ref, &ep, obj, http_server_config_out);
        if (rv != EXB_OK) {
            goto on_error_1;
        }
    }

   obj = json_get(ep.full_file, ep.tokens, "http.servers");
   if (obj && obj->type == JSMN_ARRAY) {
       rv = load_json_servers(exb_ref, &ep, obj, http_server_config_out);
       if (rv != EXB_OK) {
           goto on_error_1;
       }
   }
    
   exb_free(exb_ref, ep.tokens);
   return EXB_OK;
   on_error_1:
   exb_free(exb_ref, ep.tokens);
   return rv;
}


static int load_json_server_ssl(struct exb *exb_ref,
                             struct exb_json_parser_state *ep,
                             jsmntok_t *parent,
                             struct exb_ssl_config_entry *ssl_entry_out)
{
    exb_ssl_config_entry_init(exb_ref, ssl_entry_out);
    int port = 0;
    char *ca_path = NULL;
    char *private_key_path = NULL;
    char *public_key_path  = NULL;
    char *dh_params_path   = NULL;
    char *ssl_protocols    = NULL;
    char *ssl_ciphers      = NULL;
    bool is_default = false;
    jsmntok_t *obj = NULL;
    int rv = EXB_OK;
    if ((obj = json_get(ep->full_file, parent, "listen"))) {
        char *tmp = NULL;
        struct exb_str ip;
        int port = 0;
        if ((rv = json_get_as_string_copy(exb_ref, ep->full_file, obj, 0, &tmp)) != EXB_OK) {
            rv = EXB_CONFIG_ERROR;
            goto on_error_1;
        }
        if (split_ip_port_pair(exb_ref, tmp, &port, &ip) != 0) {
            exb_free(exb_ref, tmp);
            rv = EXB_CONFIG_ERROR;
            goto on_error_1;
        }
        
        ssl_entry_out->listen_port = port;
        ssl_entry_out->listen_ip = ip;
        exb_free(exb_ref, tmp);
    }
    if ((obj = json_get(ep->full_file, parent, "ca_path"))) {
        if ((rv = json_get_as_string_copy(exb_ref, ep->full_file, obj, 0, &ca_path)) != EXB_OK ||
            (rv = exb_str_assign_transfer(exb_ref, ca_path, &ssl_entry_out->ca_path)) != EXB_OK  )
            goto on_error_1;
    }
    if ((obj = json_get(ep->full_file, parent, "public_key"))) {
        if ((rv = json_get_as_string_copy(exb_ref, ep->full_file, obj, 0, &public_key_path)) != EXB_OK ||
            (rv = exb_str_assign_transfer(exb_ref, public_key_path, &ssl_entry_out->public_key_path)) != EXB_OK  )
            goto on_error_1;
    }
    if ((obj = json_get(ep->full_file, parent, "dh_params"))) {
        if ((rv = json_get_as_string_copy(exb_ref, ep->full_file, obj, 0, &dh_params_path)) != EXB_OK ||
            (rv = exb_str_assign_transfer(exb_ref, dh_params_path, &ssl_entry_out->dh_params_path)) != EXB_OK  )
            goto on_error_1;
    }
    if ((obj = json_get(ep->full_file, parent, "private_key"))) {
        if ((rv = json_get_as_string_copy(exb_ref, ep->full_file, obj, 0, &private_key_path)) != EXB_OK ||
            (rv = exb_str_assign_transfer(exb_ref, private_key_path, &ssl_entry_out->private_key_path)) != EXB_OK  )
            goto on_error_1;
    }
    if ((obj = json_get(ep->full_file, parent, "ssl_protocols"))) {
        if ((rv = json_get_as_string_copy(exb_ref, ep->full_file, obj, 0, &ssl_protocols)) != EXB_OK ||
            (rv = exb_str_assign_transfer(exb_ref, ssl_protocols, &ssl_entry_out->ssl_protocols)) != EXB_OK  )
            goto on_error_1;
    }
    if ((obj = json_get(ep->full_file, parent, "ssl_ciphers"))) {
        if ((rv = json_get_as_string_copy(exb_ref, ep->full_file, obj, 0, &ssl_ciphers)) != EXB_OK ||
            (rv = exb_str_assign_transfer(exb_ref, ssl_ciphers, &ssl_entry_out->ssl_ciphers)) != EXB_OK  )
            goto on_error_1;
    }

    return EXB_OK;
on_error_1:
    free(ca_path);
    free(public_key_path);
    free(private_key_path);
    free(ssl_protocols);
    free(ssl_ciphers);
    free(dh_params_path);
    return rv;
}

static int load_json_server(struct exb *exb_ref,
                             struct exb_json_parser_state *ep,
                             jsmntok_t *domain_obj,
                             struct exb_http_server_config *http_server_config_out)
{
    int rv = EXB_OK;
    struct exb_str listen_ip;
    exb_str_init_empty(&listen_ip);
    
    int port = 0;
    int is_default = 0;
    char *server_name = NULL;

    if (domain_obj->type != JSMN_OBJECT) {
        return EXB_CONFIG_ERROR;
    }
    
    jsmntok_t *obj = NULL;
    
    if ((obj = json_get(ep->full_file, domain_obj, "default"))) {
        if (json_get_as_boolean(ep->full_file, obj, &is_default) != 0)
            return EXB_CONFIG_ERROR;
    }
    if ((obj = json_get(ep->full_file, domain_obj, "listen"))) {
        char *tmp = NULL;
        if ((rv = json_get_as_string_copy(exb_ref, ep->full_file, obj, 0, &tmp)) != EXB_OK) {
            return EXB_CONFIG_ERROR;
        }
        if (split_ip_port_pair(exb_ref, tmp, &port, &listen_ip) != 0) {
            exb_free(exb_ref, tmp);
            return EXB_CONFIG_ERROR;
        }
        exb_free(exb_ref, tmp);
    }
    if ((obj = json_get(ep->full_file, domain_obj, "server_name"))) {
        if (json_get_as_string_copy(exb_ref, ep->full_file, obj, 0, &server_name) != 0)
            return EXB_CONFIG_ERROR;
    }

    jsmntok_t *ssl_obj = json_get(ep->full_file, domain_obj, "ssl");
    struct exb_ssl_config_entry *ssl_entry = NULL;

#ifdef EXB_WITH_SSL
    struct exb_ssl_config_entry ssl_entry_l;
    if (ssl_obj) {
        if (ssl_obj->type != JSMN_OBJECT) {
            exb_free(exb_ref, server_name);
            exb_str_deinit(exb_ref, &listen_ip);
            return EXB_CONFIG_ERROR;
        }
        if (load_json_server_ssl(exb_ref, ep, ssl_obj, &ssl_entry_l) != EXB_OK) {
            exb_free(exb_ref, server_name);
            exb_str_deinit(exb_ref, &listen_ip);
            return EXB_CONFIG_ERROR;
        }
        ssl_entry = &ssl_entry_l; //this is safe, because the called function copies the contents
    }
#endif //EXB_WITH_SSL
    
    rv = exb_http_server_config_add_domain_ex(exb_ref, http_server_config_out, port, &listen_ip, is_default, server_name, ssl_entry);
    exb_free(exb_ref, server_name);
    exb_str_deinit(exb_ref, &listen_ip);
    server_name = NULL;

#ifdef EXB_WITH_SSL
    if (ssl_entry)
        exb_ssl_config_entry_deinit(exb_ref, ssl_entry);
#endif //EXB_WITH_SSL

    if (rv != EXB_OK) {
        return rv;
    }

    rv = load_json_rules(exb_ref, ep, domain_obj, http_server_config_out);
    return rv;
}

static int load_json_servers(struct exb *exb_ref,
                             struct exb_json_parser_state *ep,
                             jsmntok_t *parent,
                             struct exb_http_server_config *http_server_config_out)
{
    int rv = EXB_OK;
    int offset = 1;
    for (int i=0; i<parent->size; i++) {
        jsmntok_t *server_obj = parent + offset;
        rv = load_json_server(exb_ref, ep, server_obj, http_server_config_out);
        if (rv != EXB_OK)
            goto on_error_1;
        offset += tokcount(server_obj);
    }

    return EXB_OK;
    on_error_1:
    return rv;

}

static int load_json_modules(struct exb *exb_ref,
                             struct exb_json_parser_state *ep,
                             struct exb_config *config_out,
                             struct exb_http_server_config *http_server_config_out)
{

    int integer;
    char *string;
    jsmntok_t *obj = NULL;
    int rv = EXB_OK;
    obj = json_get(ep->full_file, ep->tokens, "modules");
    if (!obj)  {
        return EXB_OK;
    }
    if (obj->type != JSMN_ARRAY) {
        return EXB_CONFIG_ERROR;
    }
        
    int offset = 1;
    for (int i=0; i<obj->size; i++) {
        if (i >= EXB_SERVER_MAX_MODULES) {
            rv = EXB_OUT_OF_RANGE_ERR;
            goto on_error_1;
        }
        jsmntok_t *module_obj = obj + offset;
        if (module_obj->type != JSMN_OBJECT) {
            rv = EXB_CONFIG_ERROR;
            goto on_error_1;
        }
        jsmntok_t *path = json_get(ep->full_file, module_obj, "path");
        jsmntok_t *args = json_get(ep->full_file, module_obj, "args");
        if (!path || !args) {
            rv = EXB_CONFIG_ERROR;
            goto on_error_1;
        }
        char *path_str = NULL;
        char *args_str = NULL;
        if (json_get_as_string_copy(exb_ref, ep->full_file, path, 0, &path_str) != EXB_OK) {
            rv = EXB_CONFIG_ERROR;
            goto on_error_2;
        }


        if (args->type == JSMN_OBJECT) {
            rv = json_to_key_eq_value_new(exb_ref, ep->full_file, args, &args_str);
            if (rv != EXB_OK) {
                rv = EXB_CONFIG_ERROR;
                goto on_error_2;
            }
        }
        else if (json_get_as_string_copy(exb_ref, ep->full_file, args, 0, &args_str) != EXB_OK){
            rv = EXB_CONFIG_ERROR;
            goto on_error_2;
        }

        struct exb_str import_name;
        exb_str_init_empty(&import_name);
        jsmntok_t *import_list_json = json_get(ep->full_file, module_obj, "import");
        struct exb_str_list *import_list = &http_server_config_out->module_specs[http_server_config_out->n_modules].import_list;
        rv = exb_str_list_init(exb_ref, import_list);
        if (rv != EXB_OK) {
            goto on_error_2;
        }
        if (import_list_json) {
            if (import_list_json->type == JSMN_STRING) {
                if ((rv = json_get_as_string_copy_1(exb_ref, ep->full_file, import_list_json, 0, &import_name)) != EXB_OK){
                    goto on_error_3;
                }
                if ((rv = exb_str_list_push(exb_ref, import_list, &import_name)) != EXB_OK){
                    exb_str_deinit(exb_ref, &import_name);
                    goto on_error_3;
                }
                rv = exb_http_server_config_add_named_empty_fptr_sink(exb_ref,
                                                                      http_server_config_out,
                                                                      import_name.str,
                                                                      i);
                if (rv != EXB_OK) {
                    goto on_error_3;
                }
            }
            else if (import_list_json->type == JSMN_ARRAY) {
                int j_offset = 1;
                for (int j=0; j < import_list_json->size; j++) {
                    jsmntok_t *import_name_obj = obj + offset;
                    if (import_name_obj->type != JSMN_STRING) {
                        rv = EXB_CONFIG_ERROR;
                        goto on_error_3;
                    }
                    offset += tokcount(import_name_obj);
                    if ((rv = json_get_as_string_copy_1(exb_ref, ep->full_file, import_name_obj, 0, &import_name)) != EXB_OK) {
                        goto on_error_3;
                    }
                    if ((rv = exb_str_list_push(exb_ref, import_list, &import_name)) != EXB_OK){
                        exb_str_deinit(exb_ref, &import_name);
                        goto on_error_3;
                    }
                    rv = exb_http_server_config_add_named_empty_fptr_sink(exb_ref,
                                                                        http_server_config_out,
                                                                        import_name.str,
                                                                        i);
                    if (rv != EXB_OK) {
                        goto on_error_3;
                    }
                }
            }
            else {
                goto on_error_3;
            }
        }
        
        exb_str_init_transfer(exb_ref, path_str, &http_server_config_out->module_specs[http_server_config_out->n_modules].module_spec);
        exb_str_init_transfer(exb_ref, args_str, &http_server_config_out->module_specs[http_server_config_out->n_modules].module_args);
        http_server_config_out->n_modules++;
        offset += tokcount(module_obj);
        continue;

        on_error_3:
        exb_str_list_deinit(exb_ref, import_list);
        on_error_2:
        exb_free(exb_ref, args_str);
        exb_free(exb_ref, path_str);
        goto on_error_1;
    }

    return EXB_OK;
    on_error_1:
    return rv;
}

static int load_json_rules(struct exb *exb_ref,
                           struct exb_json_parser_state *ep,
                           jsmntok_t *parent,
                           struct exb_http_server_config *http_server_config_out)
{
    int integer;
    char *string;
    jsmntok_t *obj = NULL;
    int rv = EXB_OK;
    int offset = 1;
    obj = json_get(ep->full_file, parent, "rules");
    if (obj) {
        if (obj->type != JSMN_ARRAY) {
            rv = EXB_CONFIG_ERROR;
            goto on_error_1;
        }
        
        for (int i=0; i<obj->size; i++) {
            if (i >= EXB_SERVER_MAX_RULES) {
                rv = EXB_OUT_OF_RANGE_ERR;
                goto on_error_1;
            }
            jsmntok_t *rule_obj = obj + offset;
            if (rule_obj->type != JSMN_OBJECT) {
                rv = EXB_CONFIG_ERROR;
                goto on_error_1;
            }

            jsmntok_t *destination_obj = json_get(ep->full_file, rule_obj, "destination");
            if (!destination_obj) {
                rv = EXB_CONFIG_ERROR;
                goto on_error_1;
            }

            /*This is one of possibly many qualifications for the rule*/
            jsmntok_t *prefix_obj = json_get(ep->full_file, rule_obj, "prefix");

            if (!destination_obj || !prefix_obj) {
                rv = EXB_CONFIG_ERROR;
                goto on_error_1;
            }

            struct exb_request_rule rule;
            struct exb_request_sink sink;
            rv = parse_rule_destination(exb_ref,
                                            ep,
                                            http_server_config_out,
                                            destination_obj,
                                            &sink);
            if (rv != EXB_OK) {
                goto on_error_1;
            }    

            rv = parse_rule(exb_ref,
                            ep,
                            http_server_config_out,
                            rule_obj,
                            0,
                            &rule);
            
            if (rv != EXB_OK) {
                exb_request_sink_deinit(exb_ref, &sink);
                goto on_error_1;
            }
            

            int sink_id = -1;
            rv = exb_http_server_config_add_sink(exb_ref, http_server_config_out, sink, &sink_id);
            if (rv != EXB_OK) {
                exb_request_sink_deinit(exb_ref, &sink);
                exb_request_rule_deinit(exb_ref, &rule);
                return rv;
            }
            rule.sink_id = sink_id;
            rv = exb_http_server_config_add_rule(exb_ref, http_server_config_out, rule);
            if (rv != EXB_OK) {
                exb_request_rule_deinit(exb_ref, &rule);
                exb_http_server_config_remove_sink(exb_ref, http_server_config_out, sink_id);
                return rv;
            }
            offset += tokcount(rule_obj);
        }
    }
    return EXB_OK;
    on_error_1:
    return rv;
}


//assumes config_out parameters were not initialized
int exb_load_configuration(struct vargstate *vg, struct exb *exb_ref, struct exb_config *config_out, struct exb_http_server_config *http_server_config_out) {
    *config_out = exb_config_default(exb_ref);
    *http_server_config_out = exb_http_server_config_default(exb_ref);
    const char *config_file = NULL;

    int explicit = varg_get_str(vg, "-c", &config_file) == VARG_OK;
    if (!explicit) {
        config_file = "config/exb.json";
    }
    
    int rv = load_json_config(config_file, exb_ref, config_out, http_server_config_out);
    if (rv != EXB_OK) {
        exb_config_deinit(exb_ref, config_out);
        exb_http_server_config_deinit(exb_ref, http_server_config_out);
    }
    return rv;
}
