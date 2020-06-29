
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <fcntl.h> /* nonblocking sockets */
#include <errno.h> 

#include "../exb_errors.h"
#include "../exb_eloop_pool.h"
#include "../exb_pcontrol.h"
#include "http_server_internal.h"
#include "http_server_events_internal.h"
#include "http_parse.h"
#include "http_request.h"
#include "http_socket_multiplexer.h"

#include "http_server_listener_select.h"
#include "http_server_listener_epoll.h"

#include "http_server_module_internal.h"

//https://www.gnu.org/software/libc/manual/html_node/Server-Example.html
//http://www.cs.tau.ac.il/~eddiea/samples/Non-Blocking/tcp-nonblocking-server.c.html

static struct exb_error make_socket (uint16_t port, int *out)
{
    int sock = socket (PF_INET, SOCK_STREAM, 0);
    struct exb_error error = {0};
    if (sock < 0) {
        error = exb_make_error(EXB_SOCKET_ERR);
        return error;
    }
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int)) < 0)
        ;//handle err

    struct sockaddr_in name;
    name.sin_family = AF_INET;
    name.sin_port = htons(port);
    name.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind (sock, (struct sockaddr *) &name, sizeof (name)) < 0) {
        error = exb_make_error(EXB_BIND_ERR);
        return error;
    }
    fcntl(sock, F_SETFL, O_NONBLOCK); /*make socket non-blocking*/
    *out = sock;
    return error;
}

static int default_handler(void *handler_state, struct exb_request_state *rqstate, int reason) {
    exb_response_append_body(rqstate, "Not found\r\n", 11);
    exb_response_end(rqstate);
    return EXB_OK;
}

static void server_postfork(void *data) {
    struct exb_server *s = data;
    //make sure epoll fd isnt inherited
    fprintf(stderr, "server_postfork %d\n", getpid());
    exb_server_listener_switch(s, s->config.polling_backend.str);
}
struct exb_error exb_server_init_with_config(struct exb_server *s, struct exb *exb_ref, struct exb_pcontrol *pcontrol, struct exb_eloop_pool *elist, struct exb_http_server_config config) {
    struct exb_error err = {0};
    s->exb                    = exb_ref;
    s->elist                  = elist;
    s->request_handler_state  = NULL;
    s->request_handler        = default_handler;
    s->n_loaded_modules       = 0;
    s->n_listen_sockets       = 0;
    s->pcontrol               = pcontrol;
    s->config                 = config;

    if (config.http_use_aio) {
        s->on_read = on_http_read_async;
        s->on_send = on_http_send_async;
    }
    else {
        s->on_read = on_http_read_sync;
        s->on_send = on_http_send_sync;
    }


    for (int i=0; i<EXB_SOCKET_MAX; i++) {
        exb_http_multiplexer_init(&s->mp[i], NULL, -1, i);
    }
    for (int i=0; i<s->config.n_domains; i++) {
        struct exb_http_domain_config *domain = &s->config.domains[i];
        /* Create sockets and set them up to accept connections. */
        bool already_defined = false;
        if (domain->http_listen_port) {
            for (int j = 0; j < i; j++) {
                if (s->listen_sockets[j].port == domain->http_listen_port) {
                    if (s->listen_sockets[j].is_ssl) {
                        exb_log_error(exb_ref,
                                      "Error: attempting to use port %d for both HTTPS and HTTP\n",
                                       domain->http_listen_port);
                        err = exb_make_error(EXB_CONFIG_ERROR);
                        goto err1;
                    }
                    //Multiple domains using same port, already initialized
                    already_defined = true;
                }
            }
            if (already_defined)
                continue;
            int socket_fd = -1;
            err = make_socket(domain->http_listen_port, &socket_fd);
            if (err.error_code) {
                err = exb_prop_error(err);
                goto err0;
            }
            else if (socket_fd >= EXB_SOCKET_MAX) {
                err = exb_make_error(EXB_OUT_OF_RANGE_ERR);
                goto err1;
            }
            s->listen_sockets[s->n_listen_sockets].socket_fd = socket_fd;
            s->listen_sockets[s->n_listen_sockets].is_ssl = false;
            s->listen_sockets[s->n_listen_sockets].port = domain->http_listen_port;
            if (listen(socket_fd, LISTEN_BACKLOG) < 0) { 
                exb_log_error(exb_ref, "Listen(%d backlog) failed, trying with 128", LISTEN_BACKLOG);
                if (listen(socket_fd, 128) < 0) {
                    err = exb_make_error(EXB_LISTEN_ERR);
                    goto err1;
                }
            }
            s->n_listen_sockets++;
        }
        
        
    }
    
    
    int rv = EXB_OK;
    for (int i=0; i<s->elist->nloops; i++) {
        s->loop_data[i].listener = NULL;
        if ((rv = exb_request_state_recycle_array_init(exb_ref, &s->loop_data[i].rq_cyc)) != EXB_OK) {
            err = exb_make_error(rv);
            for (int j = i-1; j >= 0; j--) {
                exb_request_state_recycle_array_deinit(exb_ref,  &s->loop_data[j].rq_cyc);
            }
            goto err1;
        }
    }
    //initialize s->loop_data[*].listener
    rv = exb_server_listener_switch(s, "select");
    if (rv != EXB_OK) {
        err = exb_make_error(rv);
        goto err2;
    }
    

    for (int i=0; i<config.n_modules; i++) {
        int error = exb_http_server_module_load(exb_ref, s, config.module_specs[i].module_spec.str, config.module_specs[i].module_args.str, &s->loaded_modules[s->n_loaded_modules].module, &s->loaded_modules[s->n_loaded_modules].dll_module_handle);
        if (error != EXB_OK) {
            err = exb_make_error(EXB_MODULE_LOAD_ERROR);
            goto err4;
        }
        s->n_loaded_modules++;
    }

    if (exb_pcontrol_add_postfork_hook(s->pcontrol, server_postfork, s) != EXB_OK) {
        goto err4;
    }
    
    return exb_make_error(EXB_OK);
err4:
    for (int i=0; i<s->n_loaded_modules; i++) {
        exb_http_server_module_unload(s->exb, s->loaded_modules[i].module, s->loaded_modules[i].dll_module_handle);
    }
err3:
    for (int i=0; i<s->elist->nloops; i++) {
        s->loop_data[i].listener->destroy(s->loop_data[i].listener);
    }
err2:
    for (int i = 0; i < s->elist->nloops; i++) {
        exb_request_state_recycle_array_deinit(exb_ref,  &s->loop_data[i].rq_cyc);
    }
err1:
    for (int i = 0; i < s->n_listen_sockets; i++) {
        close(s->listen_sockets[i].socket_fd);
    }
    
err0:
    for (int i=0; i<EXB_SOCKET_MAX; i++) {
        exb_http_multiplexer_deinit(&s->mp[i]);
    }
    return err;
}

/*Sets request handler for all http requests, the handler must terminate the request by sending a response*/

int exb_server_set_request_handler(struct exb_server *s, void *handler_state, exb_request_handler_func func) {
    s->request_handler = func;
    s->request_handler_state = handler_state;
    return EXB_OK;
}



struct exb_error exb_server_init(struct exb_server *s, struct exb *exb_ref, struct exb_pcontrol *pcontrol, struct exb_eloop_pool *elist, int port) {
    struct exb_http_server_config config = exb_http_server_config_default(exb_ref);
    exb_http_server_config_add_domain(exb_ref, &config, port, 1);
    struct exb_error err = exb_server_init_with_config(s, exb_ref, pcontrol, elist, config);
    if (err.error_code != EXB_OK) {
        exb_http_server_config_deinit(exb_ref, &config);
    }
    return err;
}


struct exb_http_multiplexer *exb_server_get_multiplexer(struct exb_server *s, int socket_fd) 
{
    return exb_server_get_multiplexer_i(s, socket_fd);
}


struct exb_eloop * exb_server_get_any_eloop(struct exb_server *s) {
    return exb_eloop_pool_get_any(s->elist);
}

//this is bad
static int exb_server_eloop_id(struct exb_server *s, struct exb_eloop *eloop) {
    exb_assert_h(eloop->eloop_id >= 0 && eloop->eloop_id < s->elist->nloops, "");
    return eloop->eloop_id;
}

struct exb_request_state *exb_server_new_rqstate(struct exb_server *server, struct exb_eloop *eloop, int socket_fd) {

    struct exb_request_state *st = NULL;
    int eloop_idx = exb_server_eloop_id(server, eloop);
    if (eloop_idx == -1) {
        return NULL;
    }
    if (exb_request_state_recycle_array_pop(server->exb, &server->loop_data[eloop_idx].rq_cyc, &st) != EXB_OK) {
        st = exb_malloc(server->exb, sizeof(struct exb_request_state));
    }
    if (!st) {

        return NULL;
    }
    int rv;
    if ((rv = exb_request_state_init(st, eloop, server->exb, server, socket_fd)) != EXB_OK) {
        exb_free(server->exb, st);
        return NULL;
    }

    return st;
}

void exb_server_destroy_rqstate(struct exb_server *server, struct exb_eloop *eloop, struct exb_request_state *rqstate) {

    exb_request_state_deinit(rqstate, server->exb);
    int eloop_idx = exb_server_eloop_id(server, eloop);
    if (eloop_idx == -1) {
        exb_free(server->exb, rqstate);
    }    
    if (exb_request_state_recycle_array_push(server->exb, &server->loop_data[eloop_idx].rq_cyc, rqstate) != EXB_OK) {
        exb_free(server->exb, rqstate);
    }

}


int exb_server_init_multiplexer(struct exb_server *s, struct exb_eloop *eloop, int socket_fd, struct sockaddr_in clientname) {
    int flags = fcntl(socket_fd, F_GETFL, 0);
    exb_assert_h(flags != -1, "");
    if ((flags = fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK)) == -1) {
        return EXB_SOCKET_ERR;
    }
    #ifdef EXB_SET_TCPNODELAY
        setsockopt(socket_fd, IPPROTO_TCP, TCP_NODELAY, (int [1]){1}, sizeof(int));
    #endif
    #ifdef EXB_SET_TCPQUICKACK
        setsockopt(socket_fd, IPPROTO_TCP, TCP_QUICKACK, (int [1]){1}, sizeof(int));
    #endif
    struct exb_http_multiplexer *mp = exb_server_get_multiplexer(s, socket_fd);
    if (mp == NULL)
        return EXB_SOCKET_ERR;

    int eloop_idx = exb_server_eloop_id(s, eloop);
    if (eloop_idx == -1) {
        return EXB_INVALID_ARG_ERR;
    }
    struct exb_server_listener *listener = s->loop_data[eloop_idx].listener;
    exb_assert_h(eloop_idx < s->elist->nloops, "");
    exb_eloop_pool_get_any(s->elist);
    exb_assert_h(!!eloop, "");
    exb_http_multiplexer_init(mp, eloop, eloop_idx, socket_fd);

    mp->state = EXB_MP_ACTIVE;
    
    mp->clientname = clientname;

    struct exb_request_state *rqstate = exb_server_new_rqstate(s, mp->eloop, socket_fd);
    fprintf(stderr,
            "Server: connection from host %s, port %hu. assigned to eloop: %d/%d, process: %d\n",
            inet_ntoa (clientname.sin_addr),
            ntohs (clientname.sin_port), eloop_idx, s->elist->nloops, getpid());
    listener->new_connection(listener, socket_fd);
    
    mp->currently_reading = rqstate;
    mp->wants_read = 1;
    exb_http_multiplexer_queue_response(mp, rqstate)
    RQSTATE_EVENT(stderr, "Scheduled rqstate %p to be read, because connection was just accepted\n", rqstate);
    
    return EXB_OK;
}

//this must be called from the eloop thread
void exb_server_cancel_requests(struct exb_server *s, int socket_fd) {
    /*
        deschedule all requests and defer destroying them, so that in case they're scheduled for read/write it's okay
    */
    struct exb_http_multiplexer *mp = exb_server_get_multiplexer(s, socket_fd);
    mp->wants_read  = 0;
    mp->wants_write = 0;
    mp->state = EXB_MP_CANCELLING;
    if (mp->currently_reading) {
        mp->currently_reading->is_cancelled = 1;
    }
    struct exb_request_state *next = mp->next_response;
    while (next) {
        next->is_cancelled = 1;
        next = next->next_rqstate;
    }
    struct exb_event ev;
    int rv;
    RQSTATE_EVENT(stderr, "Server marked requests as cancelled for socket %d, and scheduled HTTP_CANCEL event\n", mp->socket_fd);
    exb_event_http_init(s, &ev, EXB_HTTP_CANCEL, s, socket_fd);
    if ((rv = exb_eloop_append(mp->eloop, ev)) != EXB_OK) {
        //HMM, what to do
        abort();
    }
}
void exb_server_close_connection(struct exb_server *s, int socket_fd) {
    int eloop_idx = s->mp[socket_fd].eloop_idx;
    struct exb_server_listener *listener = s->loop_data[eloop_idx].listener;
    exb_http_multiplexer_deinit(s->mp + socket_fd);
    listener->close_connection(listener, socket_fd);
    close(socket_fd);
    RQSTATE_EVENT(stderr, "Server closed connection on socket %d\n", socket_fd);
}

void exb_server_on_read_available(struct exb_server *s, struct exb_http_multiplexer *m) {
    exb_server_on_read_available_i(s, m);
}
void exb_server_on_write_available(struct exb_server *s, struct exb_http_multiplexer *m) {
    exb_server_on_write_available_i(s, m);
}

static struct exb_error exb_server_listen_once(struct exb_server *s, int eloop_idx) {
    struct exb_error err = {0};

    struct exb_server_listener *listener = s->loop_data[eloop_idx].listener;
    exb_assert_h(!!listener, "");
    listener->listen(listener);
    
    return err;
}

/*Start server listening loop, this returns but adds itself
repeatedly to the event loop*/
void exb_server_event_listen_loop(struct exb_event ev) {
    struct exb_server *s = ev.msg.u.iip.argp;
    int eloop_idx = ev.msg.u.iip.arg1;
    struct exb_eloop *eloop = s->elist->loops[eloop_idx].loop;
    exb_assert_h(!!eloop, "");
    
    exb_server_listen_once(s, eloop_idx);
    exb_eloop_append_delayed(eloop, ev, EXB_HTTP_MIN_DELAY, 1);
}

struct exb_error exb_server_listen(struct exb_server *s) {
    //TODO: error handling
    if (!exb_pcontrol_is_single_process(s->pcontrol) &&
        !exb_pcontrol_is_worker(s->pcontrol) ) 
    {
        return exb_make_error(EXB_OK);
    }
    for (int eloop_idx=0; eloop_idx<s->elist->nloops; eloop_idx++) {
        struct exb_event new_ev = {.handle = exb_server_event_listen_loop,
                            .msg = {
                                .u = {
                                    .iip = {
                                        .argp = s,
                                        .arg1 = eloop_idx,
                                    }
                                }
                            }};
        exb_server_event_listen_loop(new_ev);
    }
    return exb_make_error(EXB_OK);
}
void exb_server_deinit(struct exb_server *s) {

    for (int i=0; i<EXB_SOCKET_MAX; i++) {
        exb_http_multiplexer_deinit(&s->mp[i]);
    }
    exb_http_server_config_deinit(s->exb, &s->config);
    for (int i=0; i<s->elist->nloops; i++) {
        s->loop_data[i].listener->destroy(s->loop_data[i].listener);
        exb_request_state_recycle_array_deinit(s->exb, &s->loop_data[i].rq_cyc);
    }
    
    for (int i=0; i<s->n_loaded_modules; i++) {
        exb_http_server_module_unload(s->exb, s->loaded_modules[i].module, s->loaded_modules[i].dll_module_handle);
    }
    for (int i=0; i<s->n_listen_sockets; i++) {
        close(s->listen_sockets[i].socket_fd);
    }
    
}



//Switch implementation of the listener, for example from select() -> epoll()
int exb_server_listener_switch(struct exb_server *s, const char *listener_name) {
    struct exb_server_listener_fdlist *fdlist = NULL;
    int rv = EXB_OK;

    struct exb_server_listener *new_listeners[EXB_MAX_ELOOPS];
    int succeeded = 0;

    for (int eloop_idx=0; eloop_idx<s->elist->nloops; eloop_idx++) {
        struct exb_eloop *eloop = s->elist->loops[eloop_idx].loop;
        struct exb_server_listener *old_listener = s->loop_data[eloop_idx].listener;
        if (old_listener != NULL) {
            rv = old_listener->get_fds(old_listener, &fdlist);
            if (rv != EXB_OK) {
                goto rollback;
            }
        }
        
        struct exb_server_listener *new_listener = NULL;
        if (strcasecmp(listener_name, "epoll") == 0) {        
            struct exb_server_listener *epoll_listener = NULL;
            rv = exb_server_listener_epoll_new(s, eloop, &epoll_listener);
            if (rv != EXB_OK) {
                exb_server_listener_fdlist_destroy(s->exb, fdlist);
                goto rollback;
            }
            new_listener = epoll_listener;
        }
        else if (strcasecmp(listener_name, "select") == 0) {
            struct exb_server_listener *select_listener = NULL;
            rv = exb_server_listener_select_new(s, eloop, &select_listener);
            if (rv != EXB_OK) {
                exb_server_listener_fdlist_destroy(s->exb, fdlist);
                goto rollback;
            }
            new_listener = select_listener;
        }
        else {
            exb_server_listener_fdlist_destroy(s->exb, fdlist);
            goto rollback;
        }
        
        if (fdlist) {
            for (int i=0; i<fdlist->len; i++) {
                new_listener->new_connection(new_listener, fdlist->fds[i]);
            }
        }
        
        if (fdlist) {
            exb_server_listener_fdlist_destroy(s->exb, fdlist);
        }
        new_listeners[succeeded] = new_listener;

        succeeded++;
    }
    exb_assert_h(succeeded == s->elist->nloops, "");
    for (int eloop_idx=0; eloop_idx<s->elist->nloops; eloop_idx++) {
        if (s->loop_data[eloop_idx].listener)
            s->loop_data[eloop_idx].listener->destroy(s->loop_data[eloop_idx].listener);
        s->loop_data[eloop_idx].listener = new_listeners[eloop_idx];
    }    
    return EXB_OK;
rollback:
    for (int i=0; i<succeeded; i++) {
        new_listeners[i]->destroy(new_listeners[i]);
    }
    return rv;
}


#ifdef EXB_WITH_SSL
    int exb_ssl_config_entries_iter(struct exb_server *server, int *iter_state, struct exb_ssl_config_entry **entry_out, int *domain_id_out)
    {
        *entry_out = NULL;
        if (*iter_state < 0) {
            return EXB_OUT_OF_RANGE_ERR;
        }
        struct exb_http_server_config *conf = &server->config;
        //skip empty domains
        while ((*iter_state) < conf->n_domains && conf->domains[*iter_state].ssl_config.listen_port == 0)
            (*iter_state)++; 
        if (*iter_state >= conf->n_domains) {
            return EXB_EOF;
        }
        *entry_out = &conf->domains[*iter_state].ssl_config;
        *domain_id_out = *iter_state;
        (*iter_state)++;
        return EXB_OK;
    }
#endif
