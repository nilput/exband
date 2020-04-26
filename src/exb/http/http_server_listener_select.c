#include <sys/select.h>
#include <sys/socket.h>
#include <errno.h>
#include "http_server_listener_select.h"

#include "../exb.h"
#include "../exb_errors.h"

#include "http_server_internal.h"
struct exb_server_listener_select {
    struct exb_server_listener head;
    struct exb_server *server; //not owned, must outlive
    struct exb_eloop *eloop;   //not owned, must outlive

    fd_set active_fd_set;
    fd_set read_fd_set;
    fd_set write_fd_set;
};

static int exb_server_listener_select_destroy(struct exb_server_listener *listener);
static int exb_server_listener_select_listen(struct exb_server_listener *listener);
static int exb_server_listener_select_close_connection(struct exb_server_listener *listener, int socket_fd);
static int exb_server_listener_select_new_connection(struct exb_server_listener *listener, int socket_fd);
static int exb_server_listener_select_get_fds(struct exb_server_listener *lis, struct exb_server_listener_fdlist **fdlist_out);

/*Possible optimization: keep a bitmask of fds that we dont care about because they're not ready, making this edge triggered*/

int exb_server_listener_select_new(struct exb_server *s, struct exb_eloop *eloop, struct exb_server_listener **listener) {
    struct exb_server_listener_select *lis = exb_malloc(s->exb, sizeof(struct exb_server_listener_select));
    if (!lis)
        return EXB_NOMEM_ERR;
    lis->head.destroy = exb_server_listener_select_destroy;
    lis->head.listen  = exb_server_listener_select_listen;
    lis->head.close_connection  = exb_server_listener_select_close_connection;
    lis->head.new_connection    = exb_server_listener_select_new_connection;
    lis->head.get_fds           = exb_server_listener_select_get_fds;
    lis->server = s;
    lis->eloop = eloop;
    
    /* Initialize the set of active sockets. */
    FD_ZERO(&lis->active_fd_set);
    FD_SET(s->listen_socket_fd, &lis->active_fd_set);

    *listener = (struct exb_server_listener *) lis;
    return EXB_OK;
}

static int exb_server_listener_select_listen(struct exb_server_listener *listener) {
    struct exb_server_listener_select *lis = (struct exb_server_listener_select *) listener;
    struct exb_server *s = lis->server;
    struct exb_error err = {0};
    lis->read_fd_set  = lis->active_fd_set;
    lis->write_fd_set = lis->active_fd_set;
    struct timeval timeout = {0, 1500}; //poll
    if (select(FD_SETSIZE, &lis->read_fd_set, &lis->write_fd_set, NULL, &timeout) < 0) {
        err = exb_make_error(EXB_SELECT_ERR);
        goto ret;
    }

    /* Service Connection requests. */
    if (FD_ISSET(s->listen_socket_fd, &lis->read_fd_set)) {
        exb_server_accept_new_connections(s, lis->eloop);
    }
    /* Service all the sockets with input pending. */
    for (int i = 0; i < FD_SETSIZE; ++i) {
        struct exb_http_multiplexer *m = exb_server_get_multiplexer_i(s, i);
        if (m->state == EXB_MP_EMPTY || m->eloop != lis->eloop)
            continue; //can be stdin or whatever, or owned by another thread
        exb_assert_h(m->wants_read || (!m->currently_reading /*destroyed*/) || m->currently_reading->is_read_scheduled, "");
        if ( FD_ISSET(i, &lis->read_fd_set) &&
             m->wants_read                    )
        {
            /* Data arriving on an already-connected socket. */
            exb_server_on_read_available(s, m);
        
        }
        if ( FD_ISSET(i, &lis->write_fd_set)   &&
             m->wants_write                      )
        {
            exb_server_on_write_available(s, m);
        }
    }
    ret:
    return err.error_code;
}
static int exb_server_listener_select_destroy(struct exb_server_listener *listener) {
    struct exb_server_listener_select *lis = (struct exb_server_listener_select *) listener;
    struct exb_server *s = lis->server;
    exb_free(s->exb, lis);
    return EXB_OK;
}
static int exb_server_listener_select_close_connection(struct exb_server_listener *listener, int socket_fd) {
    struct exb_server_listener_select *lis = (struct exb_server_listener_select *) listener;
    struct exb_server *s = lis->server;
    FD_CLR(socket_fd, &lis->active_fd_set);
    return EXB_OK;
}
static int exb_server_listener_select_new_connection(struct exb_server_listener *listener, int socket_fd) {
    struct exb_server_listener_select *lis = (struct exb_server_listener_select *) listener;
    struct exb_server *s = lis->server;
    FD_SET(socket_fd, &lis->active_fd_set);
    return EXB_OK;
}
static int exb_server_listener_select_get_fds(struct exb_server_listener *listener, struct exb_server_listener_fdlist **fdlist_out) {
    struct exb_server_listener_select *lis = (struct exb_server_listener_select *) listener;
    struct exb_server *s = lis->server;

    int *fds = exb_malloc(s->exb, FD_SETSIZE * sizeof(int));
    if (!fds) {
        return EXB_NOMEM_ERR;
    }
    struct exb_server_listener_fdlist *fdlist = exb_malloc(s->exb, sizeof(struct exb_server_listener_fdlist));
    if (!fdlist) {
        exb_free(s->exb, fdlist);
        return EXB_NOMEM_ERR;
    }
    fdlist->fds = fds;
    fdlist->len = 0;
    for (int i=0; i<FD_SETSIZE; i++) {
        if (FD_ISSET(i, &lis->active_fd_set)) {
            fdlist->fds[i] = i;
            fdlist->len++;
        }
    }
    *fdlist_out = fdlist;
    return EXB_OK;
}