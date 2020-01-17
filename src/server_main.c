#include <stdio.h>
#include<signal.h>
#include "http/http_server.h"
#include "http/http_server_listener_epoll.h"
#include "cpb_errors.h"
#include "cpb.h"
#include "cpb_eloop.h"
#include "cpb_threadpool.h"

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
    dp_end_event("main");
    dp_dump();
    exit(0);
}

static int cpb_response_append_body_cstr(struct cpb_response_state *resp, char *s) {
    return cpb_response_append_body(resp, s, strlen(s));
}

void request_handler(struct cpb_request_state *rqstate, enum cpb_request_handler_reason reason) {
    struct cpb_str path;
    //cpb_request_repr(rqstate);

    cpb_str_init(rqstate->server->cpb, &path);
    cpb_str_slice_to_copied_str(rqstate->server->cpb, rqstate->path_s, rqstate->input_buffer, &path);
    if (cpb_str_streqc(rqstate->server->cpb, &path, "/post") || cpb_str_streqc(rqstate->server->cpb, &path, "/post/")) {
        struct cpb_str key,value;
        cpb_str_init_const_str(rqstate->server->cpb, &key, "Content-Type");
        cpb_str_init_const_str(rqstate->server->cpb, &value, "text/html");
        cpb_response_append_body_cstr(&rqstate->resp, "<!DOCTYPE html>");
        cpb_response_append_body_cstr(&rqstate->resp, "<html>"
                                                      "<head>"
                                                      "    <title>CPBIN!</title>"
                                                      "</head>"
                                                      "<body>");
        if (cpb_request_has_body(rqstate)) {
            if (reason == CPB_HTTP_HANDLER_HEADERS) {
                rqstate->body_handling = CPB_HTTP_B_BUFFER;
            }
            else {
                cpb_response_append_body_cstr(&rqstate->resp, "You posted: <br>");
                cpb_response_append_body_cstr(&rqstate->resp, "<p>");

                /*XSS!*/
                if (rqstate->body_decoded.str)
                    cpb_response_append_body_cstr(&rqstate->resp, rqstate->body_decoded.str);

                cpb_response_append_body_cstr(&rqstate->resp, "</p>");
                cpb_response_append_body_cstr(&rqstate->resp, "</body>"
                                                              "</html>");
                cpb_response_end(&rqstate->resp);
            }
        }
        else {
            cpb_response_set_header(&rqstate->resp, &key, &value);
            cpb_response_append_body_cstr(&rqstate->resp, "<p> Post something! </p> <br>");
            cpb_response_append_body_cstr(&rqstate->resp, "<form action=\"\" method=\"POST\">"
                                                          "Text: <input type=\"text\" name=\"text\"><br>"
                                                          "</form>");
            cpb_response_append_body_cstr(&rqstate->resp, "</body>"
                                                          "</html>");
            cpb_response_end(&rqstate->resp);
        }
        
    }
    else {
        struct cpb_str key,value;
        cpb_str_init_const_str(rqstate->server->cpb, &key, "Content-Type");
        cpb_str_init_const_str(rqstate->server->cpb, &value, "text/plain");
        cpb_response_set_header(&rqstate->resp, &key, &value);
        cpb_response_append_body(&rqstate->resp, "Hello World!\r\n", 14);
        struct cpb_str str;
        
        cpb_str_init(rqstate->server->cpb, &str);
        
        cpb_sprintf(rqstate->server->cpb, &str, "Requested URL: '%s'", path.str);
        
        cpb_response_append_body(&rqstate->resp, str.str, str.len);
        cpb_str_deinit(rqstate->server->cpb, &str);
        cpb_response_end(&rqstate->resp);
    }
    
    cpb_str_deinit(rqstate->server->cpb, &path);
    
    
}


void task_test(struct cpb_thread *t, struct cpb_task *task) {
    fprintf(stdout, "Thread %d running task!\n", t->tid);
    fflush(stdout);
}
void set_handlers() {
   signal(SIGINT, int_handler);
   signal(SIGTERM, int_handler);
   signal(SIGHUP, int_handler);
   signal(SIGSTOP, int_handler);
   signal(SIGABRT, int_handler);
   signal(SIGSEGV, int_handler);
   signal(SIGPIPE, SIG_IGN);
}

int main(int argc, char *argv[]) {
    
    dp_register_event(__FUNCTION__);
    set_handlers();
    
    int rv;
    struct cpb_error erv = {0};
    dp_clear();
    rv = cpb_init(&cpb_state);
    ordie(rv);
    struct cpb_threadpool tp;
    rv = cpb_threadpool_init(&tp, &cpb_state);
    ordie(rv);
    rv = cpb_threadpool_set_nthreads(&tp, 4);
    ordie(rv);

    struct cpb_task task;
    task.run = task_test;
    task.err = cpb_make_error(CPB_OK);
    rv = cpb_threadpool_push_task(&tp, task);
    ordie(rv);

    

    rv = cpb_eloop_init(&eloop, &cpb_state, &tp, 2);
    ordie(rv);
    erv = cpb_server_init(&server, &cpb_state, &eloop, 8085);
    ordie(erv.error_code);

    erv.error_code = cpb_server_listener_switch_to_epoll(&server);
    ordie(erv.error_code);

    cpb_server_set_request_handler(&server, request_handler);
    erv = cpb_server_listen(&server);
    ordie(erv.error_code);
    erv = cpb_eloop_run(&eloop);
    ordie(erv.error_code);

    cpb_threadpool_deinit(&tp);
    cpb_server_deinit(&server);
    ordie(rv);
    cpb_eloop_deinit(&eloop);
    cpb_deinit(&cpb_state);
    dp_end_event(__FUNCTION__);
    dp_dump();
}
