#ifndef CPB_HTTP_SERVER_H
#define CPB_HTTP_SERVER_H




#include "../cpb_errors.h"
#include "http_request.h"
#include "http_socket_multiplexer.h"


#define LISTEN_BACKLOG 128
#define CPB_SOCKET_MAX 2048
#define CPB_HTTP_MIN_DELAY 5 //ms



define_cpb_or(int, struct cpb_or_socket);

/*
    In the beginning there was Server 
    Then Server said let there be Light

    Server -> (needs an eloop)
        has a bunch of roles:  Accept (see whether there are new connections, if so accept them)
                               Select (see whether there are sockets available for read/write, if
                                       Schedule the appropriate handler in the event loop)

        Server schedules itself to be ran in the event loop too
        it manages the lifetime of requests and stores their state, event refer to the request's state

*/

enum cpb_request_handler_reason {
    CPB_HTTP_HANDLER_HEADERS,
    CPB_HTTP_HANDLER_BODY,
};


struct cpb_server {
    struct cpb *cpb; //not owned, must outlive
    struct cpb_eloop *eloop; //not owned, must outlive
    //^will have many in the future

    void (*request_handler)(struct cpb_request_state *rqstate, enum cpb_request_handler_reason reason);
    int port;
    int listen_socket_fd;

    struct cpb_server_listener *listener;
    
    
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