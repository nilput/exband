#include <stdio.h>
#include <signal.h>

#include "exb.h"
#include "exb_errors.h"
#include "exb_eloop.h"
#include "exb_eloop_pool.h"
#include "exb_pcontrol.h"
#include "http/http_server.h"
#include "http/http_server_listener_epoll.h"

#include "util/varg.h" //argv parsing
#include "util/ini_reader.h"
#include "jsmn/jsmn.h"

void ordie(int code) {
    if (code != EXB_OK) {
        fprintf(stderr, "An error occured!: %s\n", exb_error_code_name(code));
        exit(1);
    }
}

static struct exb           exb_state;
static struct exb_eloop_pool elist;
static struct exb_pcontrol  pcontrol;
static struct exb_server    server;

void int_handler(int sig) {
    if (sig == SIGTERM)
        fprintf(stderr, "Got Ctrl-C, killing server\n");
    else 
        fprintf(stderr, "Got SIG %d, killing server\n", sig);
    fflush(stderr);
    exb_eloop_pool_stop(&elist);
    exb_pcontrol_stop(&pcontrol);
}
void set_handlers() {
   //signal(SIGINT,  int_handler);
   signal(SIGTERM, int_handler);
   //signal(SIGHUP,  int_handler);
   signal(SIGSTOP, int_handler);
   //signal(SIGABRT, int_handler);
   //signal(SIGSEGV, int_handler);
   signal(SIGPIPE, SIG_IGN);
}

struct exb_config {
    int tp_threads; //threadpool threads
    int nloops; //number of event loops
    int nproc;  //number of processes
};
struct exb_config exb_config_default(struct exb *exb_ref) {
    (void) exb_ref;
    struct exb_config conf = {0};
    conf.tp_threads = 4;
    conf.nloops = 1;
    conf.nproc = 1;
    return conf;
}

void exb_config_deinit(struct exb *exb_ref, struct exb_config *config) {
}

void stop() {
    exb_eloop_pool_deinit(&elist);
    exb_server_deinit(&server);
    exb_deinit(&exb_state);
    dp_dump();
}

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
int dump(char *js, jsmntok_t *t, int count, int indent) {
    int i, j, k;
    jsmntok_t *key;
    if (count == 0) {
        return 0;
    }
    if (t->type == JSMN_PRIMITIVE) {
        printf("%.*s", t->end - t->start, js + t->start);
        return 1;
    } else if (t->type == JSMN_STRING) {
        printf("'%.*s'", t->end - t->start, js + t->start);
        return 1;
    } else if (t->type == JSMN_OBJECT) {
        printf("\n");
        j = 0;
        for (i = 0; i < t->size; i++) {
        for (k = 0; k < indent; k++) {
            printf("  ");
        }
        key = t + 1 + j;
        j += dump(js, key, count - j, indent + 1);
        if (key->size > 0) {
            printf(": ");
            j += dump(js, t + 1 + j, count - j, indent + 1);
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
        j += dump(js, t + 1 + j, count - j, indent + 1);
        printf("\n");
        }
        return j + 1;
    }
    return 0;
}
static int load_json_config(const char *config_path, struct exb *exb_ref, struct exb_config *config_out, struct exb_http_server_config *http_server_config_out) {
    FILE *f = fopen(config_path, "r");
    if (!f) {
        return EXB_NOT_FOUND;
    }
    jsmn_parser parser;
    jsmn_init(&parser);
    char *full_file;
    size_t file_sz;
    if (exb_read_file_fully(exb_ref, f, 1024 * 128, &full_file, &file_sz) != EXB_OK) {
        fclose(f);
        return EXB_NOT_FOUND;
    }
    
    int ntokens = 0;
    jsmntok_t *tokens = NULL;
    int rv = JSMN_ERROR_NOMEM;
    while (rv == JSMN_ERROR_NOMEM) {
        ntokens = ntokens == 0 ? 8 : ntokens * 2;
        tokens = exb_realloc_f(exb_ref, tokens, sizeof(jsmntok_t) * ntokens);
        if (!tokens) {
            fclose(f);
            return EXB_NOMEM_ERR;
        }
        rv = jsmn_parse(&parser, full_file, file_sz, tokens, ntokens);
    }
    if (rv < 0) {
        fprintf(stderr, "json parse failed: %s\n", config_path);
        return EXB_INVALID_ARG_ERR;
    }
    dump(full_file, tokens, parser.toknext, 0);
    exit(1);
    /*
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
    */
   return EXB_OK;
}

//assumes config_out parameters were not initialized
static int load_configurations(struct vgstate *vg, struct exb *exb_ref, struct exb_config *config_out, struct exb_http_server_config *http_server_config_out) {
    int err;
    *config_out = exb_config_default(exb_ref);
    *http_server_config_out = exb_http_server_config_default(exb_ref);
    const char *config_file;

    int explicit = varg_get_str(vg, "-c", &config_file) == VARG_OK;
    if (!explicit) {
        FILE *tmp = NULL;
        for (int i=0; i<2 && (!tmp); i++) {
            config_file = i == 0 ? "exb.ini" : "exb.json";
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

int main(int argc, char *argv[]) {
    struct vargstate vg;
    set_handlers();
    varg_init(&vg, argc, argv);
    
    struct exb_config exb_config;
    struct exb_http_server_config exb_http_server_config;

    int rv;

    struct exb_error erv = {0};
    dp_clear();
    rv = exb_init(&exb_state);
    ordie(rv);
    rv = load_configurations(&vg, &exb_state, &exb_config, &exb_http_server_config);
    if (rv != EXB_OK) {
        fprintf(stderr, "failed to load configuration\n");
        return 1;
    }
    rv = exb_eloop_pool_init(&elist, &exb_state, exb_config.nloops);
    ordie(rv);
    rv = exb_pcontrol_init(&pcontrol, exb_config.nproc);
    ordie(rv);
    fprintf(stderr, "spawning %d eloop%c\n", exb_config.nloops, exb_config.nloops != 1 ? 's' : ' ');
    
    rv = exb_threadpool_set_nthreads(&elist.tp, exb_config.tp_threads);
    fprintf(stderr, "spawning %d thread%c\n", exb_config.tp_threads, exb_config.tp_threads != 1 ? 's' : ' ');
    ordie(rv);
    
    fprintf(stderr, "Listening on port %d\n", exb_http_server_config.http_listen_port);
    erv = exb_server_init_with_config(&server, &exb_state, &pcontrol, &elist, exb_http_server_config);
    ordie(erv.error_code);

    if (exb_strcasel_eq(exb_http_server_config.polling_backend.str, exb_http_server_config.polling_backend.len, "epoll", 5)) {
        fprintf(stderr, "using epoll\n");
        erv.error_code = exb_server_listener_switch(&server, "epoll");
        ordie(erv.error_code);
    }
    while (exb_pcontrol_running(&pcontrol)) {
        if (exb_pcontrol_is_single_process(&pcontrol) || exb_pcontrol_is_worker(&pcontrol)) {
            if (!exb_pcontrol_is_single_process(&pcontrol)) {
                rv = exb_pcontrol_child_setup(&pcontrol, &elist);
                ordie(rv);
            }
            erv = exb_server_listen(&server);
            ordie(erv.error_code);
            erv = exb_eloop_pool_run(&elist, exb_pcontrol_worker_id(&pcontrol));
            ordie(erv.error_code);
            erv = exb_eloop_pool_join(&elist);
            ordie(erv.error_code);
        }
        else if (exb_pcontrol_is_master(&pcontrol)) {
            exb_pcontrol_maintain(&pcontrol);
            exb_sleep(50);
        }
    }
    

    stop();

}