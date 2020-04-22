#include <sys/socket.h>
#include <sys/epoll.h>
#include <errno.h>
#include <unistd.h> //close()
#include "http_server_listener_epoll.h"

#include "../cpb.h"

#include "http_server.h"
#include "http_server_internal.h"
#include "http_server_events_internal.h"


#define MAX_EVENTS 2048
#define EPOLL_TIMEOUT 5

/*
TODO: Optimization: Switch to edge triggered
this requires tweaking how the eloop handles http events
*/

struct cpb_server_listener_epoll {
    struct cpb_server_listener head;
    struct cpb_server *server; //not owned, must outlive
    struct cpb_eloop *eloop;   //not owned, must outlive

    int efd;
    struct epoll_event events[MAX_EVENTS];
    int highest_fd;
};

static int cpb_server_listener_epoll_destroy(struct cpb_server_listener *listener);
static int cpb_server_listener_epoll_listen(struct cpb_server_listener *listener);
static int cpb_server_listener_epoll_close_connection(struct cpb_server_listener *listener, int socket_fd);
static int cpb_server_listener_epoll_new_connection(struct cpb_server_listener *listener, int socket_fd);
static int cpb_server_listener_epoll_get_fds(struct cpb_server_listener *lis, struct cpb_server_listener_fdlist **fdlist_out);

int cpb_server_listener_epoll_new(struct cpb_server *s, struct cpb_eloop *eloop, struct cpb_server_listener **listener) {
    struct cpb_server_listener_epoll *lis = cpb_malloc(s->cpb, sizeof(struct cpb_server_listener_epoll));
    int err = CPB_OK;
    if (!lis)
        return CPB_NOMEM_ERR;
    lis->head.destroy = cpb_server_listener_epoll_destroy;
    lis->head.listen  = cpb_server_listener_epoll_listen;
    lis->head.close_connection  = cpb_server_listener_epoll_close_connection;
    lis->head.new_connection    = cpb_server_listener_epoll_new_connection;
    lis->head.get_fds           = cpb_server_listener_epoll_get_fds;
    lis->server = s;
    lis->eloop = eloop;
    lis->highest_fd = 0;
    
    lis->efd = epoll_create1(0);
    if (lis->efd < 0) {
        err = CPB_EPOLL_INIT_ERROR;
        goto err_1;
    }
    {
        struct epoll_event event;
        event.data.fd = s->listen_socket_fd;
        //event.events = EPOLLIN | EPOLLOUT | EPOLLET;
                      //read()   write()    ^edge triggered

        
        event.events = EPOLLIN | EPOLLOUT;
                       //read()   write() 

        int rv = epoll_ctl(lis->efd, EPOLL_CTL_ADD, s->listen_socket_fd, &event);
        if (s < 0) {
            err = CPB_EPOLL_INIT_ERROR;
            goto err_2;
        }
    }

    *listener = (struct cpb_server_listener *) lis;
    return CPB_OK;
err_2:
    close(lis->efd);
err_1:
    cpb_free(s->cpb, lis);
    return err;
}

static int cpb_server_listener_epoll_listen(struct cpb_server_listener *listener) {
    struct cpb_server_listener_epoll *lis = (struct cpb_server_listener_epoll *) listener;
    struct cpb_server *s = lis->server;
    struct cpb_error err = {0};

    /*int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout);*/
    int n = epoll_wait(lis->efd, lis->events, MAX_EVENTS, EPOLL_TIMEOUT);
    if (n < 0) {
        return CPB_EPOLL_WAIT_ERROR;
    }

    int incoming_connections = 0;
    
    for (int i=0; i<n; i++) {
        struct epoll_event *ev = lis->events + i;
        struct cpb_http_multiplexer *m = cpb_server_get_multiplexer_i(s, ev->data.fd);
        if (m->state != CPB_MP_ACTIVE) {
            if (ev->data.fd == s->listen_socket_fd)
                incoming_connections = 1;
            continue; //can be stdin or whatever
        }
        cpb_assert_h(m->eloop == lis->eloop, "");
        cpb_assert_h(!!m->next_response, "");
        cpb_assert_h(m->wants_read || (!m->creading /*destroyed*/) || m->creading->is_read_scheduled, "");
        if ( (ev->events & EPOLLIN) &&
              m->wants_read           )
        {
            /* Data arriving on an already-connected socket. */
            cpb_server_on_read_available_i(s, m);
        }
        
        if ((ev->events & EPOLLOUT)           &&
             m->wants_write                     )
        {
            cpb_server_on_write_available_i(s, m);
        }
        
    }

    /* Service Connection requests. */
    if (incoming_connections) {
        cpb_server_accept_new_connections(s, lis->eloop);
    }


    
    ret:
    return err.error_code;
}
static int cpb_server_listener_epoll_destroy(struct cpb_server_listener *listener) {
    struct cpb_server_listener_epoll *lis = (struct cpb_server_listener_epoll *) listener;
    struct cpb_server *s = lis->server;

    struct epoll_event event;
    epoll_ctl(lis->efd, EPOLL_CTL_DEL, s->listen_socket_fd, &event);
    close(lis->efd);
    cpb_free(s->cpb, listener);
    
    return CPB_OK;
}
static int cpb_server_listener_epoll_close_connection(struct cpb_server_listener *listener, int socket_fd) {
    struct cpb_server_listener_epoll *lis = (struct cpb_server_listener_epoll *) listener;
    struct cpb_server *s = lis->server;

    struct epoll_event event;
    epoll_ctl(lis->efd, EPOLL_CTL_DEL, socket_fd, &event);
    if (socket_fd == lis->highest_fd)
        lis->highest_fd--; //this is obviously not fool proof

    return CPB_OK;
}
#if 1
    #define EPOLL_EVENTS (EPOLLIN | EPOLLOUT | EPOLLET)
                          //read()   write()    ^edge triggered
#else
    #define EPOLL_EVENTS (EPOLLIN | EPOLLOUT)
                          //read()   write() 
#endif

static int cpb_server_listener_epoll_new_connection(struct cpb_server_listener *listener, int socket_fd) {
    struct cpb_server_listener_epoll *lis = (struct cpb_server_listener_epoll *) listener;
    struct cpb_server *s = lis->server;

    struct epoll_event event;
    event.data.fd = socket_fd;
    event.events = EPOLL_EVENTS;

    int rv = epoll_ctl(lis->efd, EPOLL_CTL_ADD, socket_fd, &event);
    lis->highest_fd = lis->highest_fd < socket_fd ? socket_fd : lis->highest_fd;
    return rv >= 0 ? CPB_OK : CPB_EPOLL_ADD_ERROR;
}
static int cpb_server_listener_epoll_get_fds(struct cpb_server_listener *listener, struct cpb_server_listener_fdlist **fdlist_out) {
    struct cpb_server_listener_epoll *lis = (struct cpb_server_listener_epoll *) listener;
    struct cpb_server *s = lis->server;

    int *fds = NULL;
    if (lis->highest_fd > 0) {
        cpb_malloc(s->cpb, lis->highest_fd * sizeof(int));
        if (!fds) {
            return CPB_NOMEM_ERR;
        }
    }
    struct cpb_server_listener_fdlist *fdlist = cpb_malloc(s->cpb, sizeof(struct cpb_server_listener_fdlist));
    if (!fdlist) {
        cpb_free(s->cpb, fdlist);
        return CPB_NOMEM_ERR;
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
    return CPB_OK;
}
