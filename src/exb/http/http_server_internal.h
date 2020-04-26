#ifndef EXB_HTTP_SERVER_INTERNAL_H
#define EXB_HTTP_SERVER_INTERNAL_H
#include "http_server.h"

//Get the multiplexer handling a particular socket
static inline struct exb_http_multiplexer *exb_server_get_multiplexer_i(struct exb_server *s, int socket_fd) 
{
    if (socket_fd > EXB_SOCKET_MAX)
        return NULL;
    exb_assert_h(socket_fd >= 0, "invalid socket no");
    return s->mp + socket_fd;
}

/*Called whenever a listener detects new connections*/
static int exb_server_accept_new_connections(struct exb_server *s, struct exb_eloop *eloop) {
    int new_socket;
    struct sockaddr_in clientname;
    socklen_t size = sizeof(&clientname);
    int err = EXB_OK;
    for (int i=0; i<100; i++) {
        new_socket = accept(s->listen_socket_fd,
                    (struct sockaddr *) &clientname,
                    &size);
        if (new_socket < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                err = EXB_ACCEPT_ERR;
            }
            break;
        }
        if (new_socket >= EXB_SOCKET_MAX) {
            err = EXB_OUT_OF_RANGE_ERR;
            break;
        }
        struct exb_http_multiplexer *nm = exb_server_get_multiplexer_i(s, new_socket);
        exb_assert_h(nm && (nm->state == EXB_MP_EMPTY || nm->state == EXB_MP_DEAD), "");
        err = exb_server_init_multiplexer(s, eloop, new_socket, clientname);
        if (err != EXB_OK) {
            close(new_socket);
            break;
        }
    }
    return err;
}

#endif // EXB_HTTP_SERVER_INTERNAL_H