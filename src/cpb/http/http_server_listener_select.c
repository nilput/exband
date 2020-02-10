#include <sys/select.h>
#include <sys/socket.h>
#include <errno.h>
#include "http_server_listener_select.h"

#include "../cpb.h"
#include "../cpb_errors.h"

#include "http_server_internal.h"
struct cpb_server_listener_select {
    struct cpb_server_listener head;
    struct cpb_server *server; //not owned, must outlive
    struct cpb_eloop *eloop;   //not owned, must outlive

    fd_set active_fd_set;
    fd_set read_fd_set;
    fd_set write_fd_set;
};

static int cpb_server_listener_select_destroy(struct cpb_server_listener *listener);
static int cpb_server_listener_select_listen(struct cpb_server_listener *listener);
static int cpb_server_listener_select_close_connection(struct cpb_server_listener *listener, int socket_fd);
static int cpb_server_listener_select_new_connection(struct cpb_server_listener *listener, int socket_fd);
static int cpb_server_listener_select_get_fds(struct cpb_server_listener *lis, struct cpb_server_listener_fdlist **fdlist_out);

/*Possible optimization: keep a bitmask of fds that we dont care about because they're not ready, making this edge triggered*/

int cpb_server_listener_select_new(struct cpb_server *s, struct cpb_eloop *eloop, struct cpb_server_listener **listener) {
    struct cpb_server_listener_select *lis = cpb_malloc(s->cpb, sizeof(struct cpb_server_listener_select));
    if (!lis)
        return CPB_NOMEM_ERR;
    lis->head.destroy = cpb_server_listener_select_destroy;
    lis->head.listen  = cpb_server_listener_select_listen;
    lis->head.close_connection  = cpb_server_listener_select_close_connection;
    lis->head.new_connection    = cpb_server_listener_select_new_connection;
    lis->head.get_fds           = cpb_server_listener_select_get_fds;
    lis->server = s;
    lis->eloop = eloop;
    
    /* Initialize the set of active sockets. */
    FD_ZERO(&lis->active_fd_set);
    FD_SET(s->listen_socket_fd, &lis->active_fd_set);

    *listener = (struct cpb_server_listener *) lis;
    return CPB_OK;
}

static int cpb_server_listener_select_listen(struct cpb_server_listener *listener) {
    struct cpb_server_listener_select *lis = (struct cpb_server_listener_select *) listener;
    struct cpb_server *s = lis->server;
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
        cpb_server_accept_new_connections(s, lis->eloop);
    }
    /* Service all the sockets with input pending. */
    for (int i = 0; i < FD_SETSIZE; ++i) {
        struct cpb_http_multiplexer *m = cpb_server_get_multiplexer_i(s, i);
        if (m->state == CPB_MP_EMPTY || m->eloop != lis->eloop)
            continue; //can be stdin or whatever, or owned by another thread
        cpb_assert_h(m->wants_read || (!m->creading /*destroyed*/) || m->creading->is_read_scheduled, "");
        if ( FD_ISSET(i, &lis->read_fd_set) &&
             m->wants_read                    )
        {
            /* Data arriving on an already-connected socket. */
            cpb_server_on_read_available(s, m);
        
        }
        if ( FD_ISSET(i, &lis->write_fd_set)   &&
             m->wants_write                      )
        {
            cpb_server_on_write_available(s, m);
        }
    }
    ret:
    return err.error_code;
}
static int cpb_server_listener_select_destroy(struct cpb_server_listener *listener) {
    struct cpb_server_listener_select *lis = (struct cpb_server_listener_select *) listener;
    struct cpb_server *s = lis->server;
    cpb_free(s->cpb, lis);
    return CPB_OK;
}
static int cpb_server_listener_select_close_connection(struct cpb_server_listener *listener, int socket_fd) {
    struct cpb_server_listener_select *lis = (struct cpb_server_listener_select *) listener;
    struct cpb_server *s = lis->server;
    FD_CLR(socket_fd, &lis->active_fd_set);
    return CPB_OK;
}
static int cpb_server_listener_select_new_connection(struct cpb_server_listener *listener, int socket_fd) {
    struct cpb_server_listener_select *lis = (struct cpb_server_listener_select *) listener;
    struct cpb_server *s = lis->server;
    FD_SET(socket_fd, &lis->active_fd_set);
    return CPB_OK;
}
static int cpb_server_listener_select_get_fds(struct cpb_server_listener *listener, struct cpb_server_listener_fdlist **fdlist_out) {
    struct cpb_server_listener_select *lis = (struct cpb_server_listener_select *) listener;
    struct cpb_server *s = lis->server;

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
        if (FD_ISSET(i, &lis->active_fd_set)) {
            fdlist->fds[i] = i;
            fdlist->len++;
        }
    }
    *fdlist_out = fdlist;
    return CPB_OK;
}