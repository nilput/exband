#include <stdio.h>
#include <signal.h>

#include "exb.h"
#include "exb_errors.h"
#include "exb_eloop.h"
#include "exb_eloop_pool.h"
#include "exb_pcontrol.h"
#include "http/http_server.h"
#include "http/http_server_listener_epoll.h"
#include "server_config.h"

#include "util/varg.h" //argv parsing

void ordie(int code) {
    if (code != EXB_OK) {
        fprintf(stderr, "An error occured!: %s\n", exb_error_code_name(code));
        exit(1);
    }
}

static struct exb            exb_state;
static struct exb_eloop_pool elist;
static struct exb_pcontrol   pcontrol;
static struct exb_server     server;

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


void stop() {
    exb_eloop_pool_deinit(&elist);
    exb_server_deinit(&server);
    exb_deinit(&exb_state);
    dp_dump();
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
    rv = exb_load_configuration(&vg, &exb_state, &exb_config, &exb_http_server_config);
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