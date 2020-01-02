#include <sys/select.h>
#include <unistd.h>
#include "errors.h"
#include "server.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <fcntl.h> /* Added for the nonblocking socket */
//https://www.gnu.org/software/libc/manual/html_node/Server-Example.html
//http://www.cs.tau.ac.il/~eddiea/samples/Non-Blocking/tcp-nonblocking-server.c.html

#include "server_events.h"
#include "http_parse.h"
#include "http_request.h"




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
struct cpb_error cpb_server_init(struct cpb_server *s, struct cpb *cpb_ref, struct cpb_eloop *eloop, int port) {
    struct cpb_error err = {0};
    s->cpb = cpb_ref;
    s->eloop = eloop;
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

void cpb_server_close_connection(struct cpb_server *s, int socket_fd) {
    close(socket_fd);
    FD_CLR(socket_fd, &s->active_fd_set);
}

struct cpb_error cpb_server_listen_once(struct cpb_server *s) {
    struct cpb_error err = {0};
    if (listen(s->listen_socket_fd, LISTEN_BACKLOG) < 0) { 
        err = cpb_make_error(CPB_LISTEN_ERR);
        return err;
    }

    /* Block until input arrives on one or more active sockets. */
    s->read_fd_set = s->active_fd_set;
    s->write_fd_set = s->active_fd_set;
    struct timeval timeout = {0, 0}; //poll
    if (select(FD_SETSIZE, &s->read_fd_set, &s->write_fd_set, NULL, &timeout) < 0) {
        err = cpb_make_error(CPB_SELECT_ERR);
        return err;
    }

    /* Service all the sockets with input pending. */
    for (int i = 0; i < FD_SETSIZE; ++i) {
        if (FD_ISSET(i, &s->read_fd_set)) {
            if (i == s->listen_socket_fd) {
                /* Connection request on original socket. */
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
                fcntl(new_socket, F_SETFL, O_NONBLOCK); /* Change the socket into non-blocking state	*/
                struct cpb_request_state *rqstate = &s->requests[new_socket];
                cpb_request_state_init(rqstate, s, new_socket, clientname);
                fprintf(stderr,
                        "Server: connect from host %s, port %hu.\n",
                        inet_ntoa (clientname.sin_addr),
                        ntohs (clientname.sin_port));
                FD_SET(new_socket, &s->active_fd_set);
                struct cpb_event ev;
                cpb_event_http_init(&ev, new_socket, CPB_HTTP_INIT, s->requests+new_socket);
                cpb_eloop_append(s->eloop, ev);
            }
            else {
                /* Data arriving on an already-connected socket. */
                struct cpb_event ev;
                cpb_event_http_init(&ev, i, CPB_HTTP_READ, s->requests+i);
                cpb_eloop_append(s->eloop, ev);
            }
        }
        if (FD_ISSET(i, &s->write_fd_set) && s->requests[i].resp.state == CPB_HTTP_R_ST_SENDING) {
            struct cpb_event ev;
            cpb_event_http_init(&ev, i, CPB_HTTP_SEND, s->requests+i);
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
} ;

void cpb_server_deinit(struct cpb_server *s) {

}
