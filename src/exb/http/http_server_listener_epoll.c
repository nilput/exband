#include <sys/socket.h>
#include <sys/epoll.h>
#include <errno.h>
#include <unistd.h> //close()
#include "http_server_listener_epoll.h"

#include "../exb.h"

#include "http_server.h"
#include "http_server_internal.h"
#include "http_server_events_internal.h"


#define MAX_EVENTS 2048
#define EPOLL_TIMEOUT 20

/*
TODO: Optimization: Switch to edge triggered
this requires tweaking how the eloop handles http events
*/

struct exb_server_listener_epoll {
    struct exb_server_listener head;
    struct exb_server *server; //not owned, must outlive
    struct exb_eloop *eloop;   //not owned, must outlive

    int efd;
    struct epoll_event events[MAX_EVENTS];
    int highest_fd;
};

static int exb_server_listener_epoll_destroy(struct exb_server_listener *listener);
static int exb_server_listener_epoll_listen(struct exb_server_listener *listener);
static int exb_server_listener_epoll_close_connection(struct exb_server_listener *listener, int socket_fd);
static int exb_server_listener_epoll_new_connection(struct exb_server_listener *listener, int socket_fd);
static int exb_server_listener_epoll_get_fds(struct exb_server_listener *lis, struct exb_server_listener_fdlist **fdlist_out);

int exb_server_listener_epoll_new(struct exb_server *s, struct exb_eloop *eloop, struct exb_server_listener **listener) {
    struct exb_server_listener_epoll *lis = exb_malloc(s->exb, sizeof(struct exb_server_listener_epoll));
    int err = EXB_OK;
    if (!lis)
        return EXB_NOMEM_ERR;
    lis->head.destroy          = exb_server_listener_epoll_destroy;
    lis->head.listen           = exb_server_listener_epoll_listen;
    lis->head.close_connection = exb_server_listener_epoll_close_connection;
    lis->head.new_connection   = exb_server_listener_epoll_new_connection;
    lis->head.get_fds          = exb_server_listener_epoll_get_fds;
    lis->server     = s;
    lis->eloop      = eloop;
    lis->highest_fd = 0;
    
    lis->efd = epoll_create1(0);
    if (lis->efd < 0) {
        err = EXB_EPOLL_INIT_ERROR;
        goto err_1;
    }
    {
        struct epoll_event event;
        for (int i=0; i < s->n_listen_sockets; i++) {
            event.data.fd = s->listen_sockets[i].socket_fd;
            //event.events = EPOLLIN | EPOLLOUT | EPOLLET;
                        //read()   write()    ^edge triggered
            
            event.events = EPOLLIN | EPOLLOUT;
                        //read()   write() 

            int rv = epoll_ctl(lis->efd, EPOLL_CTL_ADD, s->listen_sockets[i].socket_fd, &event);
            if (s < 0) {
                err = EXB_EPOLL_INIT_ERROR;
                goto err_2;
            }
        }
    }

    *listener = (struct exb_server_listener *) lis;
    return EXB_OK;
err_2:
    close(lis->efd);
err_1:
    exb_free(s->exb, lis);
    return err;
}

static int exb_server_listener_epoll_listen(struct exb_server_listener *listener) {
    struct exb_server_listener_epoll *lis = (struct exb_server_listener_epoll *) listener;
    struct exb_server *s = lis->server;
    struct exb_error err = {0};

    /*int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout);*/
    int n = epoll_wait(lis->efd, lis->events, MAX_EVENTS, EPOLL_TIMEOUT);
    if (n < 0) {
        return EXB_EPOLL_WAIT_ERROR;
    }

    int incoming_connections = 0;
    
    for (int i=0; i<n; i++) {
        struct epoll_event *ev = lis->events + i;
        struct exb_http_multiplexer *m = exb_server_get_multiplexer_i(s, ev->data.fd);
        if (m->state != EXB_MP_ACTIVE) {
            for (int j=0; j<s->n_listen_sockets; j++) {
                if (s->listen_sockets[j].socket_fd == ev->data.fd) {
                    incoming_connections = 1;
                }
            }
            continue; //can be a listening socket
        }
        exb_assert_h(m->eloop == lis->eloop, "");
        exb_assert_h(!!m->next_response, "");
        exb_assert_h(m->wants_read || (!m->currently_reading /*destroyed*/) || m->currently_reading->is_read_scheduled, "");
        if ( (ev->events & EPOLLIN) &&
              m->wants_read           )
        {
            /* Data arriving on an already-connected socket. */
            exb_server_on_read_available_i(s, m);
        }
        
        if ((ev->events & EPOLLOUT)           &&
             m->wants_write                     )
        {
            exb_server_on_write_available_i(s, m);
        }
        
    }

    /* Service Connection requests. */
    if (incoming_connections) {
        exb_server_accept_new_connections(s, lis->eloop);
    }


    
    ret:
    return err.error_code;
}
static int exb_server_listener_epoll_destroy(struct exb_server_listener *listener) {
    struct exb_server_listener_epoll *lis = (struct exb_server_listener_epoll *) listener;
    struct exb_server *s = lis->server;

    struct epoll_event event;
    for (int i=0; i<s->n_listen_sockets; i++) {
        epoll_ctl(lis->efd, EPOLL_CTL_DEL, s->listen_sockets[i].socket_fd, &event);
    }
    close(lis->efd);
    exb_free(s->exb, listener);
    
    return EXB_OK;
}
static int exb_server_listener_epoll_close_connection(struct exb_server_listener *listener, int socket_fd) {
    struct exb_server_listener_epoll *lis = (struct exb_server_listener_epoll *) listener;
    struct exb_server *s = lis->server;

    struct epoll_event event;
    epoll_ctl(lis->efd, EPOLL_CTL_DEL, socket_fd, &event);
    if (socket_fd == lis->highest_fd)
        lis->highest_fd--; //this is obviously not fool proof

    return EXB_OK;
}
#if 1
    #define EPOLL_EVENTS (EPOLLIN | EPOLLOUT | EPOLLET)
                          //read()   write()    ^edge triggered
#else
    #define EPOLL_EVENTS (EPOLLIN | EPOLLOUT)
                          //read()   write() 
#endif

static int exb_server_listener_epoll_new_connection(struct exb_server_listener *listener, int socket_fd) {
    struct exb_server_listener_epoll *lis = (struct exb_server_listener_epoll *) listener;
    struct exb_server *s = lis->server;

    struct epoll_event event;
    event.data.fd = socket_fd;
    event.events = EPOLL_EVENTS;

    int rv = epoll_ctl(lis->efd, EPOLL_CTL_ADD, socket_fd, &event);
    lis->highest_fd = lis->highest_fd < socket_fd ? socket_fd : lis->highest_fd;
    return rv >= 0 ? EXB_OK : EXB_EPOLL_ADD_ERROR;
}
static int exb_server_listener_epoll_get_fds(struct exb_server_listener *listener, struct exb_server_listener_fdlist **fdlist_out) {
    struct exb_server_listener_epoll *lis = (struct exb_server_listener_epoll *) listener;
    struct exb_server *s = lis->server;

    int *fds = NULL;
    if (lis->highest_fd > 0) {
        exb_malloc(s->exb, lis->highest_fd * sizeof(int));
        if (!fds) {
            return EXB_NOMEM_ERR;
        }
    }
    struct exb_server_listener_fdlist *fdlist = exb_malloc(s->exb, sizeof(struct exb_server_listener_fdlist));
    if (!fdlist) {
        exb_free(s->exb, fdlist);
        return EXB_NOMEM_ERR;
    }
    fdlist->fds = fds;
    fdlist->len = 0;

    for (int i=0; i<lis->highest_fd; i++) {
        struct epoll_event event;
        event.data.fd = i;
        event.events = EPOLL_EVENTS;
        int rv = epoll_ctl(lis->efd, EPOLL_CTL_ADD, i, &event);
        
        if (rv == -1 && errno == EEXIST) {
            fdlist->fds[fdlist->len++] = i;
        }
        else if (rv == 0) {
            epoll_ctl(lis->efd, EPOLL_CTL_DEL, i, &event);
        }
    }
    *fdlist_out = fdlist;
    return EXB_OK;
}
