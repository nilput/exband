#include <sys/select.h>
#include <sys/socket.h>
#include <errno.h>
#include "http_server_listener_select.h"

#include "../cpb.h"
#include "../cpb_errors.h"

#include "http_server.h"



struct cpb_server_listener_select {
    struct cpb_server_listener head;
    fd_set active_fd_set;
    fd_set read_fd_set;
    fd_set write_fd_set;
};

static int cpb_server_listener_select_destroy(struct cpb_server *s, struct cpb_server_listener *listener);
static int cpb_server_listener_select_listen(struct cpb_server *s, struct cpb_server_listener *listener);
static int cpb_server_listener_select_close_connection(struct cpb_server *s, struct cpb_server_listener *listener, int socket_fd);
static int cpb_server_listener_select_new_connection(struct cpb_server *s, struct cpb_server_listener *listener, int socket_fd);
static int cpb_server_listener_select_get_fds(struct cpb_server *s, struct cpb_server_listener *lis, struct cpb_server_listener_fdlist **fdlist_out);

/*Possible optimization: keep a bitmask of fds that we dont care about because they're not ready, making this edge triggered*/

int cpb_server_listener_select_new(struct cpb_server *s, struct cpb_server_listener **listener) {
    struct cpb_server_listener_select *lis = cpb_malloc(s->cpb, sizeof(struct cpb_server_listener_select));
    if (!lis)
        return CPB_NOMEM_ERR;
    lis->head.destroy = cpb_server_listener_select_destroy;
    lis->head.listen  = cpb_server_listener_select_listen;
    lis->head.close_connection  = cpb_server_listener_select_close_connection;
    lis->head.new_connection    = cpb_server_listener_select_new_connection;
    lis->head.get_fds           = cpb_server_listener_select_get_fds;
    
    /* Initialize the set of active sockets. */
    FD_ZERO(&lis->active_fd_set);
    FD_SET(s->listen_socket_fd, &lis->active_fd_set);

    *listener = (struct cpb_server_listener *) lis;
    return CPB_OK;
}

static int cpb_server_listener_select_listen(struct cpb_server *s, struct cpb_server_listener *listener) {
    struct cpb_server_listener_select *lis = (struct cpb_server_listener_select *) listener;
    struct cpb_error err = {0};
    lis->read_fd_set  = lis->active_fd_set;
    lis->write_fd_set = lis->active_fd_set;
    struct timeval timeout = {0, 1500}; //poll
    if (select(FD_SETSIZE, &lis->read_fd_set, &lis->write_fd_set, NULL, &timeout) < 0) {
        err = cpb_make_error(CPB_SELECT_ERR);
        goto ret;
    }

    /* Service Connection requests. */
    if (FD_ISSET(s->listen_socket_fd, &lis->read_fd_set)) {
        //TOOD: this should accept in a loop
        int new_socket;
        struct sockaddr_in clientname;
        socklen_t size = sizeof(&clientname);
        while (1) {
            new_socket = accept(s->listen_socket_fd,
                        (struct sockaddr *) &clientname,
                        &size);
            if (new_socket < 0) {
                if (errno != EAGAIN && errno != EWOULDBLOCK) {
                    err = cpb_make_error(CPB_ACCEPT_ERR);
                    goto ret;
                }
                else {
                    break;
                }
            }
            if (new_socket >= CPB_SOCKET_MAX) {
                err = cpb_make_error(CPB_OUT_OF_RANGE_ERR);
                goto ret;
            }
            struct cpb_http_multiplexer *nm = cpb_server_get_multiplexer(s, new_socket);
            cpb_assert_h(nm && nm->state == CPB_MP_EMPTY, "");
            int rv = cpb_server_init_multiplexer(s, new_socket, clientname);
        }
    }
    /* Service all the sockets with input pending. */
    for (int i = 0; i < FD_SETSIZE; ++i) {
        if (i == s->listen_socket_fd)
            continue;
        struct cpb_http_multiplexer *m = cpb_server_get_multiplexer(s, i);
        if (m->state == CPB_MP_EMPTY)
            continue; //can be stdin or whatever
        cpb_assert_h(!!m->creading, "");
        if ( FD_ISSET(i, &lis->read_fd_set)              &&
             m->creading->istate != CPB_HTTP_I_ST_DONE &&
            !m->creading->is_read_scheduled              ) 
        {
            /* Data arriving on an already-connected socket. */
            cpb_server_on_read_available(s, m);
        
        }
        if ( FD_ISSET(i, &lis->write_fd_set)                         &&
             m->next_response                                      &&
             m->next_response->resp.state == CPB_HTTP_R_ST_SENDING &&
            !m->next_response->is_send_scheduled                      )
        {
            cpb_server_on_write_available(s, m);
        }
    }
    ret:
    return err.error_code;
}
static int cpb_server_listener_select_destroy(struct cpb_server *s, struct cpb_server_listener *listener) {
    struct cpb_server_listener_select *lis = (struct cpb_server_listener_select *) listener;
    cpb_free(s->cpb, lis);
    return CPB_OK;
}
static int cpb_server_listener_select_close_connection(struct cpb_server *s, struct cpb_server_listener *listener, int socket_fd) {
    struct cpb_server_listener_select *lis = (struct cpb_server_listener_select *) listener;
    FD_CLR(socket_fd, &lis->active_fd_set);
    return CPB_OK;
}
static int cpb_server_listener_select_new_connection(struct cpb_server *s, struct cpb_server_listener *listener, int socket_fd) {
    struct cpb_server_listener_select *lis = (struct cpb_server_listener_select *) listener;
    FD_SET(socket_fd, &lis->active_fd_set);
    return CPB_OK;
}
static int cpb_server_listener_select_get_fds(struct cpb_server *s, struct cpb_server_listener *listener, struct cpb_server_listener_fdlist **fdlist_out) {
    struct cpb_server_listener_select *lis = (struct cpb_server_listener_select *) listener;
    int *fds = cpb_malloc(s->cpb, FD_SETSIZE * sizeof(int));
    if (!fds) {
        return CPB_NOMEM_ERR;
    }
    struct cpb_server_listener_fdlist *fdlist = cpb_malloc(s->cpb, sizeof(struct cpb_server_listener_fdlist));
    if (!fdlist) {
        cpb_free(s->cpb, fdlist);
        return CPB_NOMEM_ERR;
    }
    fdlist->fds = fds;
    fdlist->len = 0;
    for (int i=0; i<FD_SETSIZE; i++) {
        if (FD_ISSET(s->listen_socket_fd, &lis->active_fd_set)) {
            fdlist->fds[i] = i;
            fdlist->len++;
        }
    }
    *fdlist_out = fdlist;
    return CPB_OK;
}