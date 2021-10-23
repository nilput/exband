#include <stdio.h>
#include <signal.h>

#include "exb.h"
#include "exb_errors.h"
#include "exb_evloop.h"
#include "exb_evloop_pool.h"
#include "exb_pcontrol.h"
#include "http/http_server.h"
#include "exb_main_config.h"

#include "util/varg.h" //argv parsing

void check_or_die(int code) {
    if (code != EXB_OK) {
        fprintf(stderr, "An error occured!: %s\n", exb_error_code_name(code));
        exit(1);
    }
}

static struct exb             exb_state;
static struct exb_evloop_pool elist;
static struct exb_pcontrol    pcontrol;
static struct exb_server      server;

void int_handler(int sig) {
    if (sig == SIGTERM)
        fprintf(stderr, "Got Ctrl-C, killing server\n");
    else 
        fprintf(stderr, "Got SIG %d, killing server\n", sig);
    fflush(stderr);
    exb_evloop_pool_stop(&elist);
    exb_pcontrol_stop(&pcontrol);
}

void setup_process_signal_handlers() {
   //signal(SIGINT,  int_handler);
   signal(SIGTERM, int_handler);
   //signal(SIGHUP,  int_handler);
   signal(SIGSTOP, int_handler);
   //signal(SIGABRT, int_handler);
   //signal(SIGSEGV, int_handler);
   signal(SIGPIPE, SIG_IGN);
}

void stop() {
    exb_evloop_pool_deinit(&elist);
    exb_server_deinit(&server);
    exb_deinit(&exb_state);
    dp_dump();
}

int main(int argc, char *argv[]) {
    struct vargstate vg;
    setup_process_signal_handlers();
    varg_init(&vg, argc, argv);
    
    struct exb_config exb_config;
    struct exb_http_server_config exb_http_server_config;

    int rv;

    struct exb_error erv = {0};
    dp_clear();
    rv = exb_init(&exb_state);
    check_or_die(rv);
    rv = exb_load_configuration(&vg, &exb_state, &exb_config, &exb_http_server_config);
    if (rv != EXB_OK) {
        fprintf(stderr, "failed to load configuration\n");
        return 1;
    }
    rv = exb_evloop_pool_init(&elist, &exb_state, exb_config.nloops);
    check_or_die(rv);
    rv = exb_pcontrol_init(&pcontrol, exb_config.nproc);
    check_or_die(rv);
    fprintf(stderr, "Starting Exband %s\n", EXBAND_VERSION_STR);
    fprintf(stderr, "\tSpawning %d event loop%c\n", exb_config.nloops, exb_config.nloops != 1 ? 's' : ' ');
    
    rv = exb_threadpool_set_nthreads(&elist.tp, exb_config.tp_threads);
    fprintf(stderr, "\tSpawning %d IO thread%c\n", exb_config.tp_threads, exb_config.tp_threads != 1 ? 's' : ' ');
    check_or_die(rv);

    erv = exb_server_init_with_config(&server, &exb_state, &pcontrol, &elist, exb_http_server_config);
    check_or_die(erv.error_code);

    for (int i=0; i<server.n_listen_sockets; i++) {
        fprintf(stderr, "\tListening on port %d%s\n",
                                            server.listen_sockets[i].port,
                                            server.listen_sockets[i].is_ssl ? " [ssl]" : "");
    }
    if (exb_strcasel_eq(exb_http_server_config.polling_backend.str, exb_http_server_config.polling_backend.len, "epoll", 5)) {
        fprintf(stderr, "\tUsing epoll\n");
        erv.error_code = exb_server_listener_switch(&server, "epoll");
        check_or_die(erv.error_code);
    }
    int cpu_count = exb_hw_cpu_count();
    while (exb_pcontrol_running(&pcontrol)) {
        if (exb_pcontrol_is_single_process(&pcontrol) || exb_pcontrol_is_worker(&pcontrol)) {
            if (!exb_pcontrol_is_single_process(&pcontrol)) {
                rv = exb_pcontrol_child_setup(&pcontrol, &elist);
                check_or_die(rv);
            }
            erv = exb_server_listen(&server);
            check_or_die(erv.error_code);
            erv = exb_evloop_pool_run(&elist, exb_pcontrol_worker_id(&pcontrol) % cpu_count);
            check_or_die(erv.error_code);
            erv = exb_evloop_pool_join(&elist);
            check_or_die(erv.error_code);
        }
        else if (exb_pcontrol_is_master(&pcontrol)) {
            exb_pcontrol_maintain(&pcontrol);
            exb_sleep(50);
        }
        else {
            fprintf(stderr, "invalid run state\n");
            break;
        }
    }
    
    stop();
}
