#include <stdio.h>
#include<signal.h>
#include "http/http_server.h"
#include "cpb_errors.h"
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

void request_handler(struct cpb_request_state *rqstate) {
    struct cpb_str key,value;
    cpb_str_init_const_str(rqstate->server->cpb, &key, "Content-Type");
    cpb_str_init_const_str(rqstate->server->cpb, &value, "text/plain");
    cpb_response_set_header(&rqstate->resp, &key, &value);
    cpb_response_append_body(&rqstate->resp, "Hello World!\r\n", 14);
    struct cpb_str str;
    struct cpb_str path;
    cpb_str_init(rqstate->server->cpb, &str);
    cpb_str_init(rqstate->server->cpb, &path);
    cpb_str_slice_to_copied_str(rqstate->server->cpb, rqstate->path_s, rqstate->input_buffer, &path);
    cpb_sprintf(rqstate->server->cpb, &str, "Requested URL: '%s'", path.str);
    
    cpb_response_append_body(&rqstate->resp, str.str, str.len);
    cpb_str_deinit(rqstate->server->cpb, &str);
    cpb_str_deinit(rqstate->server->cpb, &path);
    
    cpb_response_end(&rqstate->resp);
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
    cpb_server_set_request_handler(&server, request_handler);
    erv = cpb_server_listen(&server);
    ordie(erv.error_code);
    erv = cpb_eloop_run(&eloop);
    ordie(erv.error_code);
    
    cpb_server_deinit(&server);
    ordie(rv);
    cpb_eloop_deinit(&eloop);
    cpb_deinit(&cpb_state);
}
