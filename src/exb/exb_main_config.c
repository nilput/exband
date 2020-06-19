#include "exb.h"
#include "exb_str.h"
#include "exb_build_config.h"
#include "http/http_server_config.h"
#include "util/varg.h" //argv parsing
#include "util/ini_reader.h"
#include "jsmn/jsmn.h"

static int load_ini_config(const char *config_path, struct exb *exb_ref, struct exb_config *config_out, struct exb_http_server_config *http_server_config_out) {
    int err = EXB_OK;
    FILE *f = fopen(config_path, "r");
    if (!f) {
        return EXB_NOT_FOUND;
    }

    struct ini_config *c = ini_parse(exb_ref, f);
    if (!c) {
        fclose(f);
        return EXB_CONFIG_ERROR;
    }
    fclose(f);
    f = NULL;
    struct ini_pair *p = ini_get_value(c, "n_event_loops");
    if (p) {
        int count = atoi(c->input.str + p->value.index);
        config_out->nloops = count;
    }
    p = ini_get_value(c, "n_processes");
    if (p) {
        int count = atoi(c->input.str + p->value.index);
        config_out->nproc = count;
    }
    p = ini_get_value(c, "threadpool_size");
    if (p) {
        int count = atoi(c->input.str + p->value.index);
        config_out->tp_threads = count;
    }
    p = ini_get_value(c, "polling_backend");
    if (p) {
        err = exb_str_slice_to_copied_str(exb_ref, p->value, c->input.str, &http_server_config_out->polling_backend);
        if (err != EXB_OK)
            goto err_1;
    }
    p = ini_get_value(c, "http_port");
    if (p) {
        int port = atoi(c->input.str + p->value.index);
        http_server_config_out->http_listen_port = port;
    }
    p = ini_get_value(c, "http_aio");
    if (p) {
        int boolean = atoi(c->input.str + p->value.index);
        http_server_config_out->http_use_aio = !!boolean;
    }
    struct exb_str tmp;
    exb_str_init_empty(&tmp);
    for (int i=0; i<EXB_SERVER_MAX_MODULES; i++) {
        if (i == 0)
            err = exb_sprintf(exb_ref, &tmp, "http_server_module");
        else
            err = exb_sprintf(exb_ref, &tmp, "http_server_module_%d", i);
        if (err != EXB_OK) {
            goto err_1;
        }
        p = ini_get_value(c, tmp.str);
        if (p) {
            err = exb_str_slice_to_copied_str(exb_ref, p->value, c->input.str, &http_server_config_out->module_specs[http_server_config_out->n_modules].module_spec);
            if (err != EXB_OK)
                goto err_1;
        }
        else {
            continue;
        }
        if (i == 0)
            err = exb_sprintf(exb_ref, &tmp, "http_server_module_args");
        else
            err = exb_sprintf(exb_ref, &tmp, "http_server_module_%d_args", i);
        if (err != EXB_OK) {
            goto err_1;
        }
        p = ini_get_value(c, tmp.str);
        if (p) {
            err = exb_str_slice_to_copied_str(exb_ref, p->value, c->input.str, &http_server_config_out->module_specs[http_server_config_out->n_modules].module_args);
            if (err != EXB_OK)
                goto err_1;
        }
        http_server_config_out->n_modules++;
    }
    exb_str_deinit(exb_ref, &tmp);
    ini_destroy(exb_ref, c);
    return EXB_OK;
    err_1:
    ini_destroy(exb_ref, c);
    exb_config_deinit(exb_ref, config_out);
    exb_http_server_config_deinit(exb_ref, http_server_config_out);
    return err;
}

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
        int j = 0;
        for (int i = 0; i < t->size; i++) {
            jsmntok_t *key = t + 1 + j;
            j += tokcount(key);
            if (key->size > 0) {
                j += tokcount(t + 1 + j);
            }
        }
        return j + 1;
    } else if (t->type == JSMN_ARRAY) {
        int j = 0;
        for (int i = 0; i < t->size; i++) {
            j += tokcount(t + 1 + j);
        }
        return j + 1;
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
        int j = 0;
        for (int i = 0; i < t->size; i++) {
            jsmntok_t *key = t + 1 + j;
            jsmntok_t *val = NULL;
            j += tokcount(key);
            if (key->size > 0) {
                if (key->type == JSMN_STRING                     &&
                (key->end - key->start) == (next - path)         &&
                memcmp(key->start + json_str, keyname, next - path) == 0)
                {
                    return json_get(json_str, t + 1 + j, next[0] == '.' ? next + 1 : next);
                }
                j += tokcount(t + 1 + j);
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
//return value like strcmp
int json_token_strcmp(char *json_str, jsmntok_t *t, const char *str) {
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
                                  struct exb_config *config_in,
                                  struct exb_http_server_config *http_server_config_in,
                                  jsmntok_t *obj,
                                  struct exb_request_sink *sink_out)
{
    char *path_str = NULL;
    jsmntok_t *dtype = json_get(ep->full_file, obj, "type");
    jsmntok_t *path = json_get(ep->full_file, obj, "path");
    if (!dtype ||
        !path ||
        json_token_strcmp(ep->full_file, dtype, "filesystem") != 0 ||
        json_get_as_string_copy(exb_ref, ep->full_file, path, 0, &path_str) != EXB_OK
      ) 
    {
        return EXB_CONFIG_ERROR;
    }
    //takes ownership of path_str
    int rv = exb_request_sink_filesystem_init(exb_ref, path_str, sink_out);
    if (rv != EXB_OK) {
        exb_free(exb_ref, path_str);
        return rv;
    }
    return EXB_OK;

}

/*
initializes a request rule from a json object
*/
static int parse_rule(struct exb *exb_ref,
                      struct exb_json_parser_state *ep,
                      struct exb_config *config_in,
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
                           struct exb_config *config_out,
                           struct exb_http_server_config *http_server_config_out);
static int load_json_modules(struct exb *exb_ref,
                             struct exb_json_parser_state *ep,
                             struct exb_config *config_out,
                             struct exb_http_server_config *http_server_config_out);

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
    obj = json_get(ep.full_file, ep.tokens, "http.port");
    if (obj && json_get_as_integer(ep.full_file, obj, &integer) == 0) {
        http_server_config_out->http_listen_port = integer;
    }
    obj = json_get(ep.full_file, ep.tokens, "event.loops");
    if (obj && json_get_as_integer(ep.full_file, obj, &integer) == 0) {
        config_out->nloops = integer;
    }
    obj = json_get(ep.full_file, ep.tokens, "event.processes");
    if (obj && json_get_as_integer(ep.full_file, obj, &integer) == 0) {
        if (integer < 1) {
            integer = 1;
        }
        config_out->nproc = integer;
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
    
    
   
   rv = load_json_rules(exb_ref, &ep, config_out, http_server_config_out);
   rv = load_json_modules(exb_ref, &ep, config_out, http_server_config_out);
    
   return EXB_OK;
   on_error_1:
   exb_free(exb_ref, ep.tokens);
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
    if (obj) {
        if (obj->type != JSMN_ARRAY) {
            rv = EXB_CONFIG_ERROR;
            goto on_error_1;
        }
        
        for (int i=0; i<obj->size; i++) {
            if (i >= EXB_SERVER_MAX_MODULES - 1) {
                rv = EXB_OUT_OF_RANGE_ERR;
                goto on_error_1;
            }
            jsmntok_t *module_obj = obj + 1 + i; //1 is the offset to the next token
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
            char *path_str;
            char *args_str;
            if (json_get_as_string_copy(exb_ref, ep->full_file, path, 0, &path_str) != EXB_OK) {
                rv = EXB_CONFIG_ERROR;
                goto on_error_1;
            }


            if (args->type == JSMN_OBJECT) {
                rv = json_to_key_eq_value_new(exb_ref, ep->full_file, args, &args_str);
                if (rv != EXB_OK) {
                    rv = EXB_CONFIG_ERROR;
                    exb_free(exb_ref, path_str);
                    exb_free(exb_ref, args_str);
                    goto on_error_1;
                }
            }
            else if (json_get_as_string_copy(exb_ref, ep->full_file, args, 0, &args_str) != EXB_OK){
                exb_free(exb_ref, path_str);
                rv = EXB_CONFIG_ERROR;
                goto on_error_1;
            }
            
            exb_str_init_transfer(exb_ref, path_str, &http_server_config_out->module_specs[http_server_config_out->n_modules].module_spec);
            exb_str_init_transfer(exb_ref, args_str, &http_server_config_out->module_specs[http_server_config_out->n_modules].module_args);
            http_server_config_out->n_modules++;
        }
    }
    return EXB_OK;
    on_error_1:
    return rv;
}

static int load_json_rules(struct exb *exb_ref,
                           struct exb_json_parser_state *ep,
                           struct exb_config *config_out,
                           struct exb_http_server_config *http_server_config_out)
{
    int integer;
    char *string;
    jsmntok_t *obj = NULL;
    int rv = EXB_OK;
    obj = json_get(ep->full_file, ep->tokens, "http.rules");
    if (obj) {
        if (obj->type != JSMN_ARRAY) {
            rv = EXB_CONFIG_ERROR;
            goto on_error_1;
        }
        
        for (int i=0; i<obj->size; i++) {
            if (i >= EXB_SERVER_MAX_MODULES - 1) {
                rv = EXB_OUT_OF_RANGE_ERR;
                goto on_error_1;
            }

            jsmntok_t *rule_obj = obj + 1 + i; //1 is the offset to the next token
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
                                            config_out,
                                            http_server_config_out,
                                            destination_obj,
                                            &sink);
            if (rv != EXB_OK) {
                goto on_error_1;
            }    

            rv = parse_rule(exb_ref,
                            ep,
                            config_out,
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
        }
    }
    return EXB_OK;
    on_error_1:
    return rv;
}


//assumes config_out parameters were not initialized
int exb_load_configuration(struct vargstate *vg, struct exb *exb_ref, struct exb_config *config_out, struct exb_http_server_config *http_server_config_out) {
    int err;
    *config_out = exb_config_default(exb_ref);
    *http_server_config_out = exb_http_server_config_default(exb_ref);
    const char *config_file;

    int explicit = varg_get_str(vg, "-c", &config_file) == VARG_OK;
    if (!explicit) {
        FILE *tmp = NULL;
        //try config/exb.ini or config/exb.json as a default
        for (int i=0; i<2 && (!tmp); i++) {
            config_file = i == 0 ? "config/exb.ini" : "config/exb.json";
            FILE *tmp = fopen(config_file, "r");
            if (tmp) {
                fclose(tmp);
                break;
            }
        }
    }
    
    if (exb_strc_endswith(config_file, ".json")) {
        return load_json_config(config_file, exb_ref, config_out, http_server_config_out);
    }
    return load_ini_config(config_file, exb_ref, config_out, http_server_config_out);
}