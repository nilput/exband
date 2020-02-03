#ifndef CPB_HTTP_SERVER_H
#define CPB_HTTP_SERVER_H

#include "../cpb_errors.h"
#include "../cpb_eloop_env.h"
#include "http_request.h"
#include "http_socket_multiplexer.h"
#include "http_server_module.h"
#include "cpb_request_state_recycle_array.h"


#define LISTEN_BACKLOG 8192
#define CPB_SOCKET_MAX 8192
#define CPB_SERVER_MAX_MODULES 11
#define CPB_HTTP_MIN_DELAY 2 //ms


define_cpb_or(int, struct cpb_or_socket);

/*

    Server uses one or more event loops
        has a bunch of roles:  Accepts connections
                               Select (see whether there are sockets available for read/write, and schedule the appropriate handler to be ran in the event loop

        Server schedules itself to be ran in one of the event loops
        it manages the lifetime of requests and stores their state
*/




struct cpb_http_server_config {
    int http_listen_port;
    int http_use_aio;
    struct {
        struct cpb_str module_spec; //path:entry_name
        struct cpb_str module_args;
    } module_specs[CPB_SERVER_MAX_MODULES];
    int n_modules;
    struct cpb_str polling_backend;
};
static struct cpb_http_server_config cpb_http_server_config_default(struct cpb *cpb_ref) {
    (void) cpb_ref;
    struct cpb_http_server_config conf = {0};
    conf.http_listen_port = 80;
    conf.http_use_aio = 0;
    conf.n_modules = 0;
    cpb_str_init_const_str(&conf.polling_backend, "select");
    return conf;
}
static void cpb_http_server_config_deinit(struct cpb *cpb_ref, struct cpb_http_server_config *config) {
    cpb_assert_h(config->n_modules <= CPB_SERVER_MAX_MODULES, "");
    for (int i=0; i<config->n_modules; i++) {
        cpb_str_deinit(cpb_ref, &config->module_specs[i].module_spec);
        cpb_str_deinit(cpb_ref, &config->module_specs[i].module_args);
    }
    cpb_str_deinit(cpb_ref, &config->polling_backend);
    
}


typedef void (*cpb_server_request_handler_func)(struct cpb_request_state *rqstate, enum cpb_request_handler_reason reason);

struct cpb_server {
    struct cpb *cpb; //not owned, must outlive
    struct cpb_eloop_env *elist; //not owned, must outlive
    //^will have many in the future
    struct cpb_http_server_config config; //owned

    int port;
    int listen_socket_fd;

    struct {
        struct cpb_server_listener *listener;
        struct cpb_request_state_recycle_array rq_cyc;
    } loop_data[CPB_MAX_ELOOPS];

    /*we either have a handler module (dynamic library) or a simple request handler*/
    /*simple*/
    cpb_server_request_handler_func request_handler;
    /*dynamically loaded*/
    struct cpb_http_server_module  *handler_module; //must be present in loaded_modules too
    cpb_module_request_handler_func module_request_handler;

    struct {
        struct cpb_http_server_module *module;
        void *dll_module_handle;
    } loaded_modules[CPB_SERVER_MAX_MODULES];
    int n_loaded_modules;

    struct cpb_http_multiplexer mp[CPB_SOCKET_MAX];
};

/*
single socket -> multiple concurrent requests
        #1 [GET] Read and parsed, responded, ended
        #2 [GET] Read and parsed, not responded         (current writing rqstate)
        #2 [GET] Read and parsed, not responded         (current writing rqstate + 1)
        ...
        ...
        #N [POST] Reading, wasn't parsed yet, or parsed and is reading [POST] body, not responded (current reading rqstate)
*/
struct cpb_request_state *cpb_server_current_reading_rqstate(struct cpb_server *server, int socketfd);
struct cpb_request_state *cpb_server_current_writing_rqstate(struct cpb_server *server, int socketfd);

struct cpb_eloop * cpb_server_get_any_eloop(struct cpb_server *s);

struct cpb_request_state *cpb_server_new_rqstate(struct cpb_server *server, struct cpb_eloop *eloop, int socket_fd);
//the eloop it was associated with
void cpb_server_destroy_rqstate(struct cpb_server *server, struct cpb_eloop *eloop, struct cpb_request_state *rqstate);

struct cpb_error cpb_server_init(struct cpb_server *s, struct cpb *cpb_ref, struct cpb_eloop_env *elist, int port);
/*config is owned (moved) if initialization is successful, shouldn't be deinitialized*/
struct cpb_error cpb_server_init_with_config(struct cpb_server *s, struct cpb *cpb_ref, struct cpb_eloop_env *elist, struct cpb_http_server_config config);
struct cpb_error cpb_server_listen(struct cpb_server *s);

void cpb_server_cancel_requests(struct cpb_server *s, int socket_fd);
void cpb_server_close_connection(struct cpb_server *s, int socket_fd);
int  cpb_server_set_request_handler(struct cpb_server *s, void (*handler)(struct cpb_request_state *rqstate, enum cpb_request_handler_reason reason));
int  cpb_server_set_module_request_handler(struct cpb_server *s, struct cpb_http_server_module *mod, cpb_module_request_handler_func func);
void cpb_server_deinit(struct cpb_server *s);

struct cpb_http_multiplexer *cpb_server_get_multiplexer(struct cpb_server *s, int socket_fd);
int cpb_server_init_multiplexer(struct cpb_server *s, struct cpb_eloop *eloop, int socket_fd, struct sockaddr_in clientname);


/*for gluing the listeners*/
void cpb_server_on_read_available(struct cpb_server *s, struct cpb_http_multiplexer *m);
void cpb_server_on_write_available(struct cpb_server *s, struct cpb_http_multiplexer *m);

int cpb_server_listener_switch(struct cpb_server *s, const char *listener_name);

#endif //CPB_HTTP_SERVER_H