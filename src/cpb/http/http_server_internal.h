#ifndef CPB_HTTP_SERVER_INTERNAL_H
#define CPB_HTTP_SERVER_INTERNAL_H
#include "http_server.h"
static inline struct cpb_http_multiplexer *cpb_server_get_multiplexer_i(struct cpb_server *s, int socket_fd) 
{
    if (socket_fd > CPB_SOCKET_MAX)
        return NULL;
    cpb_assert_h(socket_fd >= 0, "invalid socket no");
    return s->mp + socket_fd;
}

static int cpb_server_accept_new_connections(struct cpb_server *s, struct cpb_eloop *eloop) {
    int new_socket;
    struct sockaddr_in clientname;
    socklen_t size = sizeof(&clientname);
    int err = CPB_OK;
    for (int i=0; i<100; i++) {
        new_socket = accept(s->listen_socket_fd,
                    (struct sockaddr *) &clientname,
                    &size);
        if (new_socket < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                err = CPB_ACCEPT_ERR;
            }
            break;
        }
        if (new_socket >= CPB_SOCKET_MAX) {
            err = CPB_OUT_OF_RANGE_ERR;
            break;
        }
        struct cpb_http_multiplexer *nm = cpb_server_get_multiplexer_i(s, new_socket);
        cpb_assert_h(nm && (nm->state == CPB_MP_EMPTY || nm->state == CPB_MP_DEAD), "");
        err = cpb_server_init_multiplexer(s, eloop, new_socket, clientname);
        if (err != CPB_OK) {
            close(new_socket);
            break;
        }
    }
    return err;
}

#endif // CPB_HTTP_SERVER_INTERNAL_H