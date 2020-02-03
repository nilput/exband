#include <stdio.h>
#include <signal.h>

#include "cpb.h"
#include "cpb_errors.h"
#include "cpb_eloop.h"
#include "cpb_eloop_env.h"
#include "http/http_server.h"
#include "http/http_server_listener_epoll.h"

#include "util/vg.h" //argv
#include "util/ini_reader.h"

void ordie(int code) {
    if (code != CPB_OK) {
        fprintf(stderr, "An error occured!: %s\n", cpb_error_code_name(code));
        exit(1);
    }
}

struct cpb cpb_state;
struct cpb_server server;
struct cpb_eloop_env elist;

void int_handler(int sig) {
    if (sig == SIGTERM)
        fprintf(stderr, "Got Ctrl-C, killing server\n");
    else 
        fprintf(stderr, "Got SIG %d, killing server\n", sig);
    fflush(stderr);
    cpb_server_deinit(&server);
    cpb_eloop_env_deinit(&elist);
    cpb_deinit(&cpb_state);
    dp_dump();

    if (sig != SIGABRT)
        exit(1);
}
void set_handlers() {
   signal(SIGINT,  int_handler);
   signal(SIGTERM, int_handler);
   signal(SIGHUP,  int_handler);
   signal(SIGSTOP, int_handler);
   //signal(SIGABRT, int_handler);
   //signal(SIGSEGV, int_handler);
   signal(SIGPIPE, SIG_IGN);
}

struct cpb_config {
    int tp_threads; //threadpool threads
    int nloops; //number of event loops
};


struct cpb_config cpb_config_default(struct cpb *cpb_ref) {
    (void) cpb_ref;
    struct cpb_config conf = {0};
    conf.tp_threads = 4;
    conf.nloops = 1;
    return conf;
}

void cpb_config_deinit(struct cpb *cpb_ref, struct cpb_config *config) {
}



//assumes config_out parameters were not initialized
static int load_configurations(struct vgstate *vg, struct cpb *cpb_ref, struct cpb_config *config_out, struct cpb_http_server_config *http_server_config_out) {
    int err;
    *config_out = cpb_config_default(cpb_ref);
    *http_server_config_out = cpb_http_server_config_default(cpb_ref);
    const char *config_file;

    int explicit = vg_get_str(vg, "-c", &config_file) == VG_OK;
    if (!explicit)
        config_file = "cpb.ini";
    FILE *f = fopen(config_file, "r");
    if (!f) {
        return CPB_NOT_FOUND;
    }
    struct ini_config *c = ini_parse(cpb_ref, f);
    if (!c) {
        fclose(f);
        return CPB_CONFIG_ERROR;
    }
    fclose(f);
    f = NULL;
    struct ini_pair *p = ini_get_value(c, "n_event_loops");
    if (p) {
        int count = atoi(c->input.str + p->value.index);
        config_out->nloops = count;
    }
    p = ini_get_value(c, "threadpool_size");
    if (p) {
        int count = atoi(c->input.str + p->value.index);
        config_out->tp_threads = count;
    }
    p = ini_get_value(c, "polling_backend");
    if (p) {
        err = cpb_str_slice_to_copied_str(cpb_ref, p->value, c->input.str, &http_server_config_out->polling_backend);
        if (err != CPB_OK)
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
    struct cpb_str tmp;
    cpb_str_init_empty(&tmp);
    for (int i=0; i<CPB_SERVER_MAX_MODULES; i++) {
        if (i == 0)
            err = cpb_sprintf(cpb_ref, &tmp, "http_server_module");
        else
            err = cpb_sprintf(cpb_ref, &tmp, "http_server_module_%d", i);
        if (err != CPB_OK) {
            goto err_1;
        }
        p = ini_get_value(c, tmp.str);
        if (p) {
            err = cpb_str_slice_to_copied_str(cpb_ref, p->value, c->input.str, &http_server_config_out->module_specs[http_server_config_out->n_modules].module_spec);
            if (err != CPB_OK)
                goto err_1;
        }
        else {
            continue;
        }
        if (i == 0)
            err = cpb_sprintf(cpb_ref, &tmp, "http_server_module_args");
        else
            err = cpb_sprintf(cpb_ref, &tmp, "http_server_module_%d_args", i);
        if (err != CPB_OK) {
            goto err_1;
        }
        p = ini_get_value(c, tmp.str);
        if (p) {
            err = cpb_str_slice_to_copied_str(cpb_ref, p->value, c->input.str, &http_server_config_out->module_specs[http_server_config_out->n_modules].module_args);
            if (err != CPB_OK)
                goto err_1;
        }
        http_server_config_out->n_modules++;
    }
    cpb_str_deinit(cpb_ref, &tmp);
    ini_destroy(cpb_ref, c);
    return CPB_OK;
    err_1:
    ini_destroy(cpb_ref, c);
    cpb_config_deinit(cpb_ref, config_out);
    cpb_http_server_config_deinit(cpb_ref, http_server_config_out);
    return err;
}

int main(int argc, char *argv[]) {
    struct vgstate vg;

    dp_register_event(__FUNCTION__);
    set_handlers();
    vg_init(&vg, argc, argv);
    
    struct cpb_config cpb_config;
    struct cpb_http_server_config cpb_http_server_config;

    int rv;

    struct cpb_error erv = {0};
    dp_clear();
    rv = cpb_init(&cpb_state);
    ordie(rv);
    rv = load_configurations(&vg, &cpb_state, &cpb_config, &cpb_http_server_config);
    if (rv != CPB_OK) {
        fprintf(stderr, "failed to load configuration");
    }

    
    
    rv = cpb_eloop_env_init(&elist, &cpb_state, cpb_config.nloops);
    ordie(rv);
    fprintf(stderr, "spawning %d eloop%c\n", cpb_config.nloops, cpb_config.nloops != 1 ? 's' : ' ');
    
    rv = cpb_threadpool_set_nthreads(&elist.tp, cpb_config.tp_threads);
    fprintf(stderr, "spawning %d thread%c\n", cpb_config.tp_threads, cpb_config.tp_threads != 1 ? 's' : ' ');
    ordie(rv);
    
    fprintf(stderr, "Listening on port %d\n", cpb_http_server_config.http_listen_port);
    erv = cpb_server_init_with_config(&server, &cpb_state, &elist, cpb_http_server_config);
    ordie(erv.error_code);

    if (cpb_strcasel_eq(cpb_http_server_config.polling_backend.str, cpb_http_server_config.polling_backend.len, "epoll", 5)) {
        fprintf(stderr, "using epoll\n");
        erv.error_code = cpb_server_listener_switch(&server, "epoll");
        ordie(erv.error_code);
    }
    
    erv = cpb_server_listen(&server);
    ordie(erv.error_code);

    erv = cpb_eloop_env_run(&elist);
    ordie(erv.error_code);

    erv = cpb_eloop_env_join(&elist);
    ordie(erv.error_code);

    cpb_eloop_env_deinit(&elist);

    cpb_server_deinit(&server);
    ordie(rv);
    cpb_deinit(&cpb_state);
    dp_end_event(__FUNCTION__);
    dp_dump();
}