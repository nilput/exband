#include <sys/socket.h>
#include <sys/epoll.h>
#include <errno.h>
#include <unistd.h> //close()
#include "http_server_listener_epoll.h"

#include "../cpb.h"

#include "http_server.h"


#define MAX_EVENTS 8192
#define EPOLL_TIMEOUT 1

/*
TODO: Optimization: Switch to edge triggered
this requires tweaking how the eloop handles http events
*/

struct cpb_server_listener_epoll {
    struct cpb_server_listener head;
    int efd;
    struct epoll_event events[MAX_EVENTS];
};

static int cpb_server_listener_epoll_destroy(struct cpb_server *s, struct cpb_server_listener *listener);
static int cpb_server_listener_epoll_listen(struct cpb_server *s, struct cpb_server_listener *listener);
static int cpb_server_listener_epoll_close_connection(struct cpb_server *s, struct cpb_server_listener *listener, int socket_fd);
static int cpb_server_listener_epoll_new_connection(struct cpb_server *s, struct cpb_server_listener *listener, int socket_fd);
static int cpb_server_listener_epoll_get_fds(struct cpb_server *s, struct cpb_server_listener *lis, struct cpb_server_listener_fdlist **fdlist_out);

int cpb_server_listener_epoll_new(struct cpb_server *s, struct cpb_server_listener **listener) {
    struct cpb_server_listener_epoll *lis = cpb_malloc(s->cpb, sizeof(struct cpb_server_listener_epoll));
    int err = CPB_OK;
    if (!lis)
        return CPB_NOMEM_ERR;
    lis->head.destroy = cpb_server_listener_epoll_destroy;
    lis->head.listen  = cpb_server_listener_epoll_listen;
    lis->head.close_connection  = cpb_server_listener_epoll_close_connection;
    lis->head.new_connection    = cpb_server_listener_epoll_new_connection;
    lis->head.get_fds           = cpb_server_listener_epoll_get_fds;
    
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

static int cpb_server_listener_epoll_listen(struct cpb_server *s, struct cpb_server_listener *listener) {
    struct cpb_server_listener_epoll *lis = (struct cpb_server_listener_epoll *) listener;
    struct cpb_error err = {0};

    /*int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout);*/
    int n = epoll_wait(lis->efd, lis->events, MAX_EVENTS, EPOLL_TIMEOUT);
    if (n < 0) {
        return CPB_EPOLL_WAIT_ERROR;
    }
    int incoming_connections = 0;
    for (int w=0; w<2; w++) {
        for (int i=0; i<n; i++) {
            struct epoll_event *ev = lis->events + i;
            if (ev->data.fd == s->listen_socket_fd) {
                incoming_connections = 1;
                continue;
            }
            struct cpb_http_multiplexer *m = cpb_server_get_multiplexer(s, ev->data.fd);
            if (m->state != CPB_MP_ACTIVE)
                continue; //can be stdin or whatever
            cpb_assert_h(!!m->creading, "");
            if ( w == 0                                     &&
                (ev->events & EPOLLIN)                      &&
                 m->creading->istate != CPB_HTTP_I_ST_DONE  &&
                !m->creading->is_read_scheduled                ) 
            {
                /* Data arriving on an already-connected socket. */
                cpb_server_on_read_available(s, m);
            
            }
            if ( w == 1                                                &&
                (ev->events & EPOLLOUT)                                &&
                 m->next_response                                      &&
                 m->next_response->resp.state == CPB_HTTP_R_ST_SENDING &&
                !m->next_response->is_send_scheduled                      )
            {
                cpb_server_on_write_available(s, m);
            }
           
        }
    }

    /* Service Connection requests. */
    if (incoming_connections) {
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
            cpb_assert_h(nm && (nm->state == CPB_MP_EMPTY || nm->state == CPB_MP_DEAD), "");
            int rv = cpb_server_init_multiplexer(s, new_socket, clientname);
        }
    }

    
    ret:
    return err.error_code;
}
static int cpb_server_listener_epoll_destroy(struct cpb_server *s, struct cpb_server_listener *listener) {
    struct cpb_server_listener_epoll *lis = (struct cpb_server_listener_epoll *) listener;

    struct epoll_event event;
    epoll_ctl(lis->efd, EPOLL_CTL_DEL, s->listen_socket_fd, &event);
    close(lis->efd);
    cpb_free(s->cpb, listener);
    
    return CPB_OK;
}
static int cpb_server_listener_epoll_close_connection(struct cpb_server *s, struct cpb_server_listener *listener, int socket_fd) {
    struct cpb_server_listener_epoll *lis = (struct cpb_server_listener_epoll *) listener;
    
    struct epoll_event event;
    epoll_ctl(lis->efd, EPOLL_CTL_DEL, socket_fd, &event);

    return CPB_OK;
}
static int cpb_server_listener_epoll_new_connection(struct cpb_server *s, struct cpb_server_listener *listener, int socket_fd) {
    struct cpb_server_listener_epoll *lis = (struct cpb_server_listener_epoll *) listener;

    struct epoll_event event;
    event.data.fd = socket_fd;

    //event.events = EPOLLIN | EPOLLOUT | EPOLLET;
                   //read()   write()    ^edge triggered

    event.events = EPOLLIN | EPOLLOUT;
                   //read()   write() 

    int rv = epoll_ctl(lis->efd, EPOLL_CTL_ADD, socket_fd, &event);
    return rv >= 0 ? CPB_OK : CPB_EPOLL_ADD_ERROR;
}
static int cpb_server_listener_epoll_get_fds(struct cpb_server *s, struct cpb_server_listener *lis, struct cpb_server_listener_fdlist **fdlist_out) {
    *fdlist_out = NULL;
    return CPB_UNSUPPORTED;
}
int cpb_server_listener_switch_to_epoll(struct cpb_server *s) {
    struct cpb_server_listener_fdlist *fdlist = NULL;
    int rv = s->listener->get_fds(s, s->listener, &fdlist);
    if (rv != CPB_OK)
        return rv;
    
    struct cpb_server_listener *epoll_listener = NULL;
    rv = cpb_server_listener_epoll_new(s, &epoll_listener);
    if (rv != CPB_OK) {
        cpb_server_listener_fdlist_destroy(s->cpb, fdlist);
        return rv;
    }
    
    for (int i=0; i<fdlist->len; i++) {
        cpb_server_listener_epoll_new_connection(s, epoll_listener, fdlist->fds[i]);
    }
    s->listener->destroy(s, s->listener);
    s->listener = epoll_listener;
    cpb_server_listener_fdlist_destroy(s->cpb, fdlist);
    return CPB_OK;
}