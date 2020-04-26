#ifndef EXB_HTTP_SERVER_H
#define EXB_HTTP_SERVER_H

#include "../exb_errors.h"
#include "../exb_config.h"
#include "http_request.h"
#include "http_socket_multiplexer.h"
#include "http_server_module.h"
#include "exb_request_state_recycle_array.h"


/*

    Server uses one or more event loops
        has a bunch of roles:  Accepts connections
                               Select (see whether there are sockets available for read/write, and schedule the appropriate handler to be ran in the event loop

        Server schedules itself to be ran in one of the event loops
        it manages the lifetime of requests and stores their state
*/

struct exb_pcontrol;
struct exb_eloop_pool;

struct exb_http_server_config {
    int http_listen_port;
    int http_use_aio;
    struct {
        struct exb_str module_spec; //path:entry_name
        struct exb_str module_args;
    } module_specs[EXB_SERVER_MAX_MODULES];
    int n_modules;
    struct exb_str polling_backend;
};
static struct exb_http_server_config exb_http_server_config_default(struct exb *exb_ref) {
    (void) exb_ref;
    struct exb_http_server_config conf = {0};
    conf.http_listen_port = 80;
    conf.http_use_aio = 0;
    conf.n_modules = 0;
    exb_str_init_const_str(&conf.polling_backend, "select");
    return conf;
}
static void exb_http_server_config_deinit(struct exb *exb_ref, struct exb_http_server_config *config) {
    exb_assert_h(config->n_modules <= EXB_SERVER_MAX_MODULES, "");
    for (int i=0; i<config->n_modules; i++) {
        exb_str_deinit(exb_ref, &config->module_specs[i].module_spec);
        exb_str_deinit(exb_ref, &config->module_specs[i].module_args);
    }
    exb_str_deinit(exb_ref, &config->polling_backend);    
}


typedef void (*exb_server_request_handler_func)(struct exb_request_state *rqstate, enum exb_request_handler_reason reason);

struct exb_server {
    struct exb *exb; //not owned, must outlive
    struct exb_eloop_pool *elist; //not owned, must outlive
    struct exb_pcontrol *pcontrol; //not owned, must outlive

    void (*on_read)(struct exb_event ev);
    void (*on_send)(struct exb_event ev);

    int port;
    int listen_socket_fd;

    struct {
        struct exb_server_listener *listener;
        struct exb_request_state_recycle_array rq_cyc;
    } loop_data[EXB_MAX_ELOOPS];

    /*we either have a handler module (dynamic library) or a simple request handler*/
    /*simple*/
    exb_server_request_handler_func request_handler;
    /*dynamically loaded*/
    struct exb_http_server_module  *handler_module; //must be present in loaded_modules too
    exb_module_request_handler_func module_request_handler;

    struct exb_http_server_config config; //owned

    struct {
        struct exb_http_server_module *module;
        void *dll_module_handle;
    } loaded_modules[EXB_SERVER_MAX_MODULES];
    int n_loaded_modules;

    struct exb_http_multiplexer mp[EXB_SOCKET_MAX] EXB_ALIGN(64);
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
struct exb_request_state *exb_server_current_reading_rqstate(struct exb_server *server, int socketfd);
struct exb_request_state *exb_server_current_writing_rqstate(struct exb_server *server, int socketfd);

struct exb_eloop * exb_server_get_any_eloop(struct exb_server *s);

struct exb_request_state *exb_server_new_rqstate(struct exb_server *server, struct exb_eloop *eloop, int socket_fd);
//the eloop it was associated with
void exb_server_destroy_rqstate(struct exb_server *server, struct exb_eloop *eloop, struct exb_request_state *rqstate);

struct exb_error exb_server_init(struct exb_server *s, struct exb *exb_ref, struct exb_pcontrol *pcontrol, struct exb_eloop_pool *elist, int port);
/*config is owned (moved) if initialization is successful, shouldn't be deinitialized*/

struct exb_error exb_server_init_with_config(struct exb_server *s, struct exb *exb_ref, struct exb_pcontrol *pcontrol, struct exb_eloop_pool *elist, struct exb_http_server_config config);
struct exb_error exb_server_listen(struct exb_server *s);

void exb_server_cancel_requests(struct exb_server *s, int socket_fd);
void exb_server_close_connection(struct exb_server *s, int socket_fd);
int  exb_server_set_request_handler(struct exb_server *s, void (*handler)(struct exb_request_state *rqstate, enum exb_request_handler_reason reason));
int  exb_server_set_module_request_handler(struct exb_server *s, struct exb_http_server_module *mod, exb_module_request_handler_func func);
void exb_server_deinit(struct exb_server *s);

struct exb_http_multiplexer *exb_server_get_multiplexer(struct exb_server *s, int socket_fd);
int exb_server_init_multiplexer(struct exb_server *s, struct exb_eloop *eloop, int socket_fd, struct sockaddr_in clientname);
/*for gluing the listeners*/
void exb_server_on_read_available(struct exb_server *s, struct exb_http_multiplexer *m);
void exb_server_on_write_available(struct exb_server *s, struct exb_http_multiplexer *m);

int exb_server_listener_switch(struct exb_server *s, const char *listener_name);

#endif //EXB_HTTP_SERVER_H
