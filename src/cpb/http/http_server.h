#ifndef CPB_HTTP_SERVER_H
#define CPB_HTTP_SERVER_H




#include "../cpb_errors.h"
#include "http_request.h"
#include "http_socket_multiplexer.h"
#include "http_handler_module.h"
#include "cpb_request_state_recycle_array.h"


#define LISTEN_BACKLOG 128
#define CPB_SOCKET_MAX 8192
#define CPB_HTTP_MIN_DELAY 10 //ms



define_cpb_or(int, struct cpb_or_socket);

/*

    Server uses one or more event loops
        has a bunch of roles:  Accepts connections
                               Select (see whether there are sockets available for read/write, and schedule the appropriate handler to be ran in the event loop

        Server schedules itself to be ran in the event loop too
        it manages the lifetime of requests and stores their state
*/




struct cpb_http_server_config {
    int http_listen_port;
    int http_use_aio;
    struct cpb_str http_handler_module;
    struct cpb_str http_handler_module_args;
    struct cpb_str polling_backend;
};
static struct cpb_http_server_config cpb_http_server_config_default(struct cpb *cpb_ref) {
    (void) cpb_ref;
    struct cpb_http_server_config conf = {0};
    conf.http_listen_port = 80;
    conf.http_use_aio = 0;
    cpb_str_init_const_str(&conf.polling_backend, "select");
    cpb_str_init_const_str(&conf.http_handler_module, "");
    cpb_str_init_const_str(&conf.http_handler_module_args, "");
    return conf;
}
static void cpb_http_server_config_deinit(struct cpb *cpb_ref, struct cpb_http_server_config *config) {
    cpb_str_deinit(cpb_ref, &config->polling_backend);
    cpb_str_deinit(cpb_ref, &config->http_handler_module);
    cpb_str_deinit(cpb_ref, &config->http_handler_module_args);
}


struct cpb_server {
    struct cpb *cpb; //not owned, must outlive
    struct cpb_eloop *eloop; //not owned, must outlive
    //^will have many in the future
    struct cpb_http_server_config config; //owned

    /*we either have a handler module (dynamic library) or a simple request handler*/
    
    void (*request_handler)(struct cpb_request_state *rqstate, enum cpb_request_handler_reason reason);

    struct cpb_http_handler_module  *handler_module;
    void *dll_module_handle;

    int port;
    int listen_socket_fd;

    struct cpb_server_listener *listener;
    
    struct cpb_request_state_recycle_array rq_cyc;

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

struct cpb_request_state *cpb_server_new_rqstate(struct cpb_server *server, struct cpb_eloop *eloop, int socket_fd);
void cpb_server_destroy_rqstate(struct cpb_server *server, struct cpb_request_state *rqstate);

struct cpb_error cpb_server_init(struct cpb_server *s, struct cpb *cpb_ref, struct cpb_eloop *eloop, int port);
/*config is owned (moved) if initialization is successful, shouldn't be deinitialized*/
struct cpb_error cpb_server_init_with_config(struct cpb_server *s, struct cpb *cpb_ref, struct cpb_eloop *eloop, struct cpb_http_server_config config);
struct cpb_error cpb_server_listen(struct cpb_server *s);

void cpb_server_cancel_requests(struct cpb_server *s, int socket_fd);
void cpb_server_close_connection(struct cpb_server *s, int socket_fd);
int  cpb_server_set_request_handler(struct cpb_server *s, void (*handler)(struct cpb_request_state *rqstate, enum cpb_request_handler_reason reason));
void cpb_server_deinit(struct cpb_server *s);

struct cpb_http_multiplexer *cpb_server_get_multiplexer(struct cpb_server *s, int socket_fd);
int cpb_server_init_multiplexer(struct cpb_server *s, int socket_fd, struct sockaddr_in clientname);


/*for gluing the listeners*/
void cpb_server_on_read_available(struct cpb_server *s, struct cpb_http_multiplexer *m);
void cpb_server_on_write_available(struct cpb_server *s, struct cpb_http_multiplexer *m);

#endif //CPB_HTTP_SERVER_H