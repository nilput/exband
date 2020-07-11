#ifndef EXB_HTTP_SERVER_H
#define EXB_HTTP_SERVER_H

#include "../exb_errors.h"
#include "../exb_log.h"
#include "../exb_build_config.h"
#include "http_request_def.h"
#include "http_request_rules.h"
#include "http_socket_multiplexer.h"
#include "http_server_module.h"
#include "http_server_config.h"
#include "http_request_state_recycle_array.h"
#include "http_server_def.h"



/*
    Server uses one or more event loops
        has a bunch of roles:  Accepts connections
                               Select (see whether there are sockets available for read/write, and schedule the appropriate handler to be ran in the event loop

        Schedules itself to be ran in one of the event loops to call epoll/select
        Manages the lifetime of requests and stores their state
        Stores loaded modules
*/


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
struct exb_eloop * exb_server_get_eloop(struct exb_server *s, int eloop_id);

struct exb_request_state *exb_server_new_rqstate(struct exb_server *server, struct exb_eloop *eloop, int socket_fd);
//the eloop it was associated with
void exb_server_destroy_rqstate(struct exb_server *server, struct exb_eloop *eloop, struct exb_request_state *rqstate);

struct exb_error exb_server_init(struct exb_server *s, struct exb *exb_ref, struct exb_pcontrol *pcontrol, struct exb_eloop_pool *elist, int port);
/*config is owned (moved) if initialization is successful, shouldn't be deinitialized*/

struct exb_error exb_server_init_with_config(struct exb_server *s, struct exb *exb_ref, struct exb_pcontrol *pcontrol, struct exb_eloop_pool *elist, struct exb_http_server_config config);


//interface is copied, this should be called by the SSL module
int exb_server_set_ssl_interface(struct exb_server *s, struct exb_ssl_interface *ssl_if);

struct exb_error exb_server_listen(struct exb_server *s);

void exb_server_cancel_requests(struct exb_server *s, int socket_fd);
void exb_server_close_connection(struct exb_server *s, int socket_fd);
/*Sets request handler for all http requests, the handler must terminate the request by sending a response*/
int  exb_server_set_request_handler(struct exb_server *s, void *handler_state, exb_request_handler_func handler_func);
void exb_server_deinit(struct exb_server *s);

struct exb_http_multiplexer *exb_server_get_multiplexer(struct exb_server *s, int socket_fd);
int exb_server_init_multiplexer(struct exb_server *s, struct exb_eloop *eloop, int socket_fd, bool is_ssl, struct sockaddr_in clientname);
/*for gluing the listeners*/
void exb_server_on_read_available(struct exb_server *s, struct exb_http_multiplexer *m);
void exb_server_on_write_available(struct exb_server *s, struct exb_http_multiplexer *m);

int exb_server_listener_switch(struct exb_server *s, const char *listener_name);

#endif //EXB_HTTP_SERVER_H
