#include <stdio.h>
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
int main(int argc, char *argv[]) {
    struct cpb cpb_state;
    struct cpb_server server;
    struct cpb_eloop eloop;
    
    int rv;
    struct cpb_error erv = {0};
    rv = cpb_init(&cpb_state);
    ordie(rv);
    rv = cpb_eloop_init(&eloop, &cpb_state, 2);
    ordie(rv);
    erv = cpb_server_init(&server, &cpb_state, &eloop, 8085);
    ordie(rv);
    erv = cpb_server_listen(&server);
    erv = cpb_eloop_run(&eloop);
    
    cpb_server_deinit(&server);
    ordie(rv);
    cpb_eloop_deinit(&eloop);
}
