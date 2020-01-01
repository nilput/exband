#include <stdio.h>
#include<signal.h>
#include "server.h"
#include "errors.h"
#include "cpb.h"
#include "eloop.h"

void ordie(int code) {
    if (code != CPB_OK) {
        fprintf(stderr, "An error occured!\n");
        exit(1);
    }
}

struct cpb cpb_state;
struct cpb_server server;
struct cpb_eloop eloop;

void int_handler(int dummy) {
    fprintf(stderr, "Got Ctrl-C, killing server\n");
    fflush(stderr);
    cpb_server_deinit(&server);
    cpb_eloop_deinit(&eloop);
    cpb_deinit(&cpb_state);
    exit(0);
}

int main(int argc, char *argv[]) {
    

   signal(SIGINT, int_handler);
   signal(SIGTERM, int_handler);
   signal(SIGHUP, int_handler);
   signal(SIGSTOP, int_handler);
   signal(SIGABRT, int_handler);
   signal(SIGSEGV, int_handler);

    
    int rv;
    struct cpb_error erv = {0};
    rv = cpb_init(&cpb_state);
    ordie(rv);
    rv = cpb_eloop_init(&eloop, &cpb_state, 2);
    ordie(rv);
    erv = cpb_server_init(&server, &cpb_state, &eloop, 8085);
    ordie(erv.error_code);
    erv = cpb_server_listen(&server);
    ordie(erv.error_code);
    erv = cpb_eloop_run(&eloop);
    ordie(erv.error_code);
    
    cpb_server_deinit(&server);
    ordie(rv);
    cpb_eloop_deinit(&eloop);
    cpb_deinit(&cpb_state);
}
