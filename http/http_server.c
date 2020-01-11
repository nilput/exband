#include <sys/select.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <fcntl.h> /* nonblocking sockets */

#include "../cpb_errors.h"
#include "http_server.h"
#include "http_server_events.h"
#include "http_parse.h"
#include "http_request.h"
#include "http_socket_multiplexer.h"

//https://www.gnu.org/software/libc/manual/html_node/Server-Example.html
//http://www.cs.tau.ac.il/~eddiea/samples/Non-Blocking/tcp-nonblocking-server.c.html



static struct cpb_or_socket make_socket (uint16_t port)
{
    int sock = socket (PF_INET, SOCK_STREAM, 0);
    struct cpb_or_socket rv = {0};
    if (sock < 0) {
        rv.error = cpb_make_error(CPB_SOCKET_ERR);
        return rv;
    }
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int)) < 0)
        ;//handle err

    /* Give the socket a name. */
    struct sockaddr_in name;
    name.sin_family = AF_INET;
    name.sin_port = htons(port);
    name.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind (sock, (struct sockaddr *) &name, sizeof (name)) < 0) {
        rv.error = cpb_make_error(CPB_BIND_ERR);
        return rv;
    }
    fcntl(sock, F_SETFL, O_NONBLOCK); /* Change the socket into non-blocking state	*/
    rv.value = sock;
    return rv;
}

static void default_handler(struct cpb_request_state *rqstate, enum cpb_request_handler_reason reason) {
    cpb_response_append_body(&rqstate->resp, "Not found\r\n", 11);
    cpb_response_end(&rqstate->resp);
}

struct cpb_error cpb_server_init(struct cpb_server *s, struct cpb *cpb_ref, struct cpb_eloop *eloop, int port) {
    struct cpb_error err = {0};
    s->cpb = cpb_ref;
    s->eloop = eloop;
    s->request_handler = default_handler;
    for (int i=0; i<CPB_SOCKET_MAX; i++) {
        cpb_http_multiplexer_init(&s->mp[i]);
    }
    /* Create the socket and set it up to accept connections. */
    struct cpb_or_socket or_socket = make_socket(port);
    if (or_socket.error.error_code) {
        return cpb_prop_error(or_socket.error);
    }
    if (or_socket.value >= CPB_SOCKET_MAX) {
        return cpb_make_error(CPB_OUT_OF_RANGE_ERR);
    }
    

    int socket_fd = or_socket.value;
    s->listen_socket_fd = socket_fd;

    /* Initialize the set of active sockets. */
    FD_ZERO(&s->active_fd_set);
    FD_SET(s->listen_socket_fd, &s->active_fd_set);

    return err;
}


struct cpb_http_multiplexer *cpb_server_get_multiplexer(struct cpb_server *s, int socket_fd) 
{
    if (socket_fd > CPB_SOCKET_MAX)
        return NULL;
    cpb_assert_h(socket_fd >= 0, ""), "invalid socket no";
    return s->mp + socket_fd;
}

struct cpb_request_state *cpb_server_new_rqstate(struct cpb_server *server, int socket_fd) {
    void *p = cpb_malloc(server->cpb, sizeof(struct cpb_request_state));
    if (!p) {
        return NULL;
    }
    struct cpb_request_state *st = p;
    cpb_request_state_init(st, server->cpb, server, socket_fd);
    return st;
}
void cpb_server_destroy_rqstate(struct cpb_server *server, struct cpb_request_state *rqstate) {
    cpb_request_state_deinit(rqstate, server->cpb);
}

int cpb_server_init_multiplexer(struct cpb_server *s, int socket_fd, struct sockaddr_in clientname) {
    
    fcntl(socket_fd, F_SETFL, O_NONBLOCK); /* Change the socket into non-blocking state */
    struct cpb_http_multiplexer *mp = cpb_server_get_multiplexer(s, socket_fd);
    cpb_http_multiplexer_init(mp);
    mp->state = CPB_MP_ACTIVE;
    if (mp == NULL)
        return CPB_SOCKET_ERR;
    mp->clientname = clientname;
    struct cpb_request_state *rqstate = cpb_server_new_rqstate(s, socket_fd);
    fprintf(stderr,
            "Server: connect from host %s, port %hu.\n",
            inet_ntoa (clientname.sin_addr),
            ntohs (clientname.sin_port));
    
    FD_SET(socket_fd, &s->active_fd_set);
    mp->creading = rqstate;
    cpb_http_multiplexer_queue_response(mp, rqstate);
    
    struct cpb_event ev;
    cpb_event_http_init(&ev, socket_fd, CPB_HTTP_INIT, rqstate);
    cpb_eloop_append(s->eloop, ev);
    return CPB_OK;
}

void cpb_server_close_connection(struct cpb_server *s, int socket_fd) {
    close(socket_fd);
    FD_CLR(socket_fd, &s->active_fd_set);
    cpb_http_multiplexer_deinit(s->mp + socket_fd);
}


struct cpb_error cpb_server_listen_once(struct cpb_server *s) {
    struct cpb_error err = {0};
    if (listen(s->listen_socket_fd, LISTEN_BACKLOG) < 0) { 
        err = cpb_make_error(CPB_LISTEN_ERR);
        return err;
    }

    
    s->read_fd_set = s->active_fd_set;
    s->write_fd_set = s->active_fd_set;
    struct timeval timeout = {0, 0}; //poll
    if (select(FD_SETSIZE, &s->read_fd_set, &s->write_fd_set, NULL, &timeout) < 0) {
        err = cpb_make_error(CPB_SELECT_ERR);
        return err;
    }

    /* Service Connection requests. */
    if (FD_ISSET(s->listen_socket_fd, &s->read_fd_set)) {
        //TOOD: this should accept in a loop
        int new_socket;
        struct sockaddr_in clientname;
        socklen_t size = sizeof(&clientname);
        new_socket = accept(s->listen_socket_fd,
                    (struct sockaddr *) &clientname,
                    &size);
        if (new_socket < 0) {
            err = cpb_make_error(CPB_ACCEPT_ERR);
            return err;
        }
        if (new_socket >= CPB_SOCKET_MAX) {
            err = cpb_make_error(CPB_OUT_OF_RANGE_ERR);
            return err;
        }
        struct cpb_http_multiplexer *nm = cpb_server_get_multiplexer(s, new_socket);
        cpb_assert_h(nm && nm->state == CPB_MP_EMPTY, "");
        int rv = cpb_server_init_multiplexer(s, new_socket, clientname);
    }
    /* Service all the sockets with input pending. */
    for (int i = 0; i < FD_SETSIZE; ++i) {
        if (i == s->listen_socket_fd)
            continue;
        struct cpb_http_multiplexer *m = cpb_server_get_multiplexer(s, i);
        
        if (FD_ISSET(i, &s->read_fd_set)) 
        {
            /* Data arriving on an already-connected socket. */
            struct cpb_event ev;
            cpb_assert_h(m && m->state == CPB_MP_ACTIVE, "");
            cpb_assert_h(!!m->creading, "");
            cpb_event_http_init(&ev, i, CPB_HTTP_READ, m->creading);
            cpb_eloop_append(s->eloop, ev);
        
        }
        if (FD_ISSET(i, &s->write_fd_set)                       &&
            m->next_response                                    &&
            m->next_response->resp.state == CPB_HTTP_R_ST_SENDING ) 
        {
            struct cpb_event ev;
            cpb_event_http_init(&ev, i, CPB_HTTP_SEND, m->next_response);
            cpb_eloop_append(s->eloop, ev);
        }
    }
    return err;
}

struct cpb_event_handler_itable cpb_server_event_handler;
void cpb_server_ev_listen_loop(struct cpb_event ev) {
    struct cpb_server *s = ev.msg.argp;
    cpb_server_listen_once(s);
    struct cpb_event new_ev = {.itable = &cpb_server_event_handler,
                           .msg = {
                            .argp = s
                           }};
    cpb_eloop_append_delayed(s->eloop, new_ev, CPB_HTTP_MIN_DELAY);
    
}

struct cpb_error cpb_server_listen(struct cpb_server *s) {
    struct cpb_event new_ev = {.itable = &cpb_server_event_handler,
                           .msg = {
                            .argp = s
                           }};
    cpb_server_ev_listen_loop(new_ev);
    return cpb_make_error(CPB_OK);
}

void cpb_server_ev_destroy(struct cpb_event ev) {
    struct cpb_server *s = ev.msg.argp;
}

struct cpb_event_handler_itable cpb_server_event_handler = {
    .handle = cpb_server_ev_listen_loop,
    .destroy = cpb_server_ev_destroy,
};

void cpb_server_deinit(struct cpb_server *s) {

}

int cpb_server_set_request_handler(struct cpb_server *s, void (*handler)(struct cpb_request_state *rqstate, enum cpb_request_handler_reason reason)) {
    s->request_handler = handler;
    return CPB_OK;
}