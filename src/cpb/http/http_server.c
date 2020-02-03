
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <fcntl.h> /* nonblocking sockets */
#include <errno.h> 

#include "../cpb_errors.h"
#include "../cpb_eloop_env.h"
#include "http_server.h"
#include "http_server_internal.h"
#include "http_server_events.h"
#include "http_parse.h"
#include "http_request.h"
#include "http_socket_multiplexer.h"

#include "http_server_listener_select.h"

#include "http_server_module_internal.h"

//https://www.gnu.org/software/libc/manual/html_node/Server-Example.html
//http://www.cs.tau.ac.il/~eddiea/samples/Non-Blocking/tcp-nonblocking-server.c.html



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

static void default_handler(struct cpb_request_state *rqstate, enum cpb_request_handler_reason reason) {
    cpb_response_append_body(&rqstate->resp, "Not found\r\n", 11);
    cpb_response_end(&rqstate->resp);
}
static void module_handler(struct cpb_request_state *rqstate, enum cpb_request_handler_reason reason) {
    cpb_assert_h(rqstate->server && rqstate->server->handler_module && rqstate->server->module_request_handler, "");
    //TODO: dont ignore, also too many indirections that can be easily avoided!
    struct cpb_server *s = rqstate->server;
    int ignored = s->module_request_handler(s->handler_module, rqstate, reason);
}


struct cpb_error cpb_server_init_with_config(struct cpb_server *s, struct cpb *cpb_ref, struct cpb_eloop_env *elist, struct cpb_http_server_config config) {
    struct cpb_error err = {0};
    s->cpb = cpb_ref;
    s->elist = elist;
    s->request_handler   = default_handler;
    s->handler_module    = NULL;
    s->module_request_handler = NULL;
    s->n_loaded_modules = 0;

    s->config = config;


    for (int i=0; i<CPB_SOCKET_MAX; i++) {
        cpb_http_multiplexer_init(&s->mp[i], NULL, -1, i);
    }
    /* Create the socket and set it up to accept connections. */
    struct cpb_or_socket or_socket = make_socket(s->config.http_listen_port);
    if (or_socket.error.error_code) {
        err = cpb_prop_error(or_socket.error);
        goto err0;
    }
    else if (or_socket.value >= CPB_SOCKET_MAX) {
        err = cpb_make_error(CPB_OUT_OF_RANGE_ERR);
        goto err1;
    }
    
    int socket_fd = or_socket.value;
    s->listen_socket_fd = socket_fd;

    if (listen(s->listen_socket_fd, LISTEN_BACKLOG) < 0) { 
        fprintf(stderr, "Listen(%d backlog) failed, trying with 128", LISTEN_BACKLOG);
        if (listen(s->listen_socket_fd, 128) < 0) {
            err = cpb_make_error(CPB_LISTEN_ERR);
            goto err1;
        }
    }
    int rv = CPB_OK;
    for (int i=0; i<s->elist->nloops; i++) {
        s->loop_data[i].listener = NULL;
        if ((rv = cpb_request_state_recycle_array_init(cpb_ref, &s->loop_data[i].rq_cyc)) != CPB_OK) {
            err = cpb_make_error(rv);
            for (int j = i-1; j >= 0; j--) {
                cpb_request_state_recycle_array_deinit(cpb_ref,  &s->loop_data[j].rq_cyc);
            }
            goto err1;
        }
    }
    //initialize s->loop_data[*].listener
    rv = cpb_server_listener_switch(s, "select");
    if (rv != CPB_OK) {
        err = cpb_make_error(rv);
        goto err2;
    }
    

    for (int i=0; i<config.n_modules; i++) {
        int error = cpb_http_server_module_load(cpb_ref, s, config.module_specs[i].module_spec.str, config.module_specs[i].module_args.str, &s->loaded_modules[s->n_loaded_modules].module, &s->loaded_modules[s->n_loaded_modules].dll_module_handle);
        if (error != CPB_OK) {
            err = cpb_make_error(CPB_MODULE_LOAD_ERROR);
            goto err4;
        }
        s->n_loaded_modules++;
    }
    
    return cpb_make_error(CPB_OK);
err4:
    for (int i=0; i<s->n_loaded_modules; i++) {
        cpb_http_server_module_unload(s->cpb, s->loaded_modules[i].module, s->loaded_modules[i].dll_module_handle);
    }
err3:
    for (int i=0; i<s->elist->nloops; i++) {
        s->loop_data[i].listener->destroy(s->loop_data[i].listener);
    }
err2:
    for (int i = 0; i < s->elist->nloops; i++) {
        cpb_request_state_recycle_array_deinit(cpb_ref,  &s->loop_data[i].rq_cyc);
    }
err1:
    close(s->listen_socket_fd);
err0:
    for (int i=0; i<CPB_SOCKET_MAX; i++) {
        cpb_http_multiplexer_deinit(&s->mp[i]);
    }
    return err;
}


        
int cpb_server_set_module_request_handler(struct cpb_server *s, struct cpb_http_server_module *mod, cpb_module_request_handler_func func) {
    s->request_handler = module_handler;
    s->handler_module = mod;
    s->module_request_handler = func;
    return CPB_OK;
}

struct cpb_error cpb_server_init(struct cpb_server *s, struct cpb *cpb_ref, struct cpb_eloop_env *elist, int port) {
    struct cpb_http_server_config config = cpb_http_server_config_default(cpb_ref);
    config.http_listen_port = port;
    struct cpb_error err = cpb_server_init_with_config(s, cpb_ref, elist, config);
    if (err.error_code != CPB_OK) {
        cpb_http_server_config_deinit(cpb_ref, &config);
    }
    return err;
}


struct cpb_http_multiplexer *cpb_server_get_multiplexer(struct cpb_server *s, int socket_fd) 
{
    return cpb_server_get_multiplexer_i(s, socket_fd);
}


struct cpb_eloop * cpb_server_get_any_eloop(struct cpb_server *s) {
    return cpb_eloop_env_get_any(s->elist);
}

//this is bad
static int cpb_server_find_eloop(struct cpb_server *s, struct cpb_eloop *eloop) {
    for (int i=0; i<s->elist->nloops; i++) {
        if (s->elist->loops[i].loop == eloop)
            return i;
    }
    return -1;
}

struct cpb_request_state *cpb_server_new_rqstate(struct cpb_server *server, struct cpb_eloop *eloop, int socket_fd) {
    dp_register_event(__FUNCTION__);
    struct cpb_request_state *st = NULL;
    int eloop_idx = cpb_server_find_eloop(server, eloop);
    if (eloop_idx == -1)
        return CPB_INVALID_ARG_ERR;
    if (cpb_request_state_recycle_array_pop(server->cpb, &server->loop_data[eloop_idx].rq_cyc, &st) != CPB_OK) {
        st = cpb_malloc(server->cpb, sizeof(struct cpb_request_state));
    }
    if (!st) {
        dp_end_event(__FUNCTION__);
        return NULL;
    }
    int rv;
    if ((rv = cpb_request_state_init(st, eloop, server->cpb, server, socket_fd)) != CPB_OK) {
        cpb_free(server->cpb, st);
        return NULL;
    }
    dp_end_event(__FUNCTION__);
    return st;
}
void cpb_server_destroy_rqstate(struct cpb_server *server, struct cpb_eloop *eloop, struct cpb_request_state *rqstate) {
    dp_register_event(__FUNCTION__);
    cpb_request_state_deinit(rqstate, server->cpb);
    int eloop_idx = cpb_server_find_eloop(server, eloop);
    if (eloop_idx == -1) {
        cpb_free(server->cpb, rqstate);
    }    
    if (cpb_request_state_recycle_array_push(server->cpb, &server->loop_data[eloop_idx].rq_cyc, rqstate) != CPB_OK) {
        cpb_free(server->cpb, rqstate);
    }
    dp_end_event(__FUNCTION__);
}


int cpb_server_init_multiplexer(struct cpb_server *s, struct cpb_eloop *eloop, int socket_fd, struct sockaddr_in clientname) {
    int flags = fcntl(socket_fd, F_GETFL, 0);
    cpb_assert_h(flags != -1, "");
    if ((flags = fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK)) == -1) {
        return CPB_SOCKET_ERR;
    }
    setsockopt(socket_fd, IPPROTO_TCP, TCP_NODELAY, (int [1]){1}, sizeof(int));
    struct cpb_http_multiplexer *mp = cpb_server_get_multiplexer(s, socket_fd);
    if (mp == NULL)
        return CPB_SOCKET_ERR;

    int eloop_idx = cpb_server_find_eloop(s, eloop);
    if (eloop_idx == -1) {
        return CPB_INVALID_ARG_ERR;
    }
    struct cpb_server_listener *listener = s->loop_data[eloop_idx].listener;
    cpb_assert_h(eloop_idx < s->elist->nloops, "");
    cpb_eloop_env_get_any(s->elist);
    cpb_assert_h(!!eloop, "");
    cpb_http_multiplexer_init(mp, eloop, eloop_idx, socket_fd);

    mp->state = CPB_MP_ACTIVE;
    
    mp->clientname = clientname;
    struct cpb_request_state *rqstate = cpb_server_new_rqstate(s, mp->eloop, socket_fd);
    fprintf(stderr,
            "Server: connection from host %s, port %hu. assigned to eloop: %d/%d\n",
            inet_ntoa (clientname.sin_addr),
            ntohs (clientname.sin_port), eloop_idx, s->elist->nloops);
    
    
    listener->new_connection(listener, socket_fd);
    
    mp->creading = rqstate;
    cpb_http_multiplexer_queue_response(mp, rqstate);
    
    
    RQSTATE_EVENT(stderr, "Scheduled rqstate %p to be read, because connection was just accepted\n", rqstate);
    
    return CPB_OK;
}

//this must be called from the eloop thread
void cpb_server_cancel_requests(struct cpb_server *s, int socket_fd) {
    /*
        deschedule all requests and defer destroying them
    */
    struct cpb_http_multiplexer *mp = cpb_server_get_multiplexer(s, socket_fd);
    mp->state = CPB_MP_CANCELLING;
    if (mp->creading) {
        mp->creading->is_cancelled = 1;
    }
    struct cpb_request_state *next = mp->next_response;
    while (next) {
        next->is_cancelled = 1;
        next = next->next_rqstate;
    }
    struct cpb_event ev;
    int rv;
    cpb_event_http_init(&ev, CPB_HTTP_CANCEL, s, socket_fd);
    if ((rv = cpb_eloop_append(mp->eloop, ev)) != CPB_OK) {
        //HMM, what to do
        abort();
    }
}
void cpb_server_close_connection(struct cpb_server *s, int socket_fd) {
    int eloop_idx = s->mp[socket_fd].eloop_idx;
    struct cpb_server_listener *listener = s->loop_data[eloop_idx].listener;
    listener->close_connection(listener, socket_fd);
    close(socket_fd);
    cpb_http_multiplexer_deinit(s->mp + socket_fd);
}

void cpb_server_on_read_available(struct cpb_server *s, struct cpb_http_multiplexer *m) {
    cpb_server_on_read_available_i(s, m);
}
void cpb_server_on_write_available(struct cpb_server *s, struct cpb_http_multiplexer *m) {
    cpb_server_on_write_available_i(s, m);
}

struct cpb_error cpb_server_listen_once(struct cpb_server *s, int eloop_idx) {
    struct cpb_error err = {0};
    dp_register_event(__FUNCTION__);
    struct cpb_server_listener *listener = s->loop_data[eloop_idx].listener;
    cpb_assert_h(!!listener, "");
    listener->listen(listener);
    
ret:
    dp_end_event(__FUNCTION__);
    return err;
}

struct cpb_event_handler_itable cpb_server_event_handler;
void cpb_server_ev_listen_loop(struct cpb_event ev) {
    struct cpb_server *s = ev.msg.u.iip.argp;
    int eloop_idx = ev.msg.u.iip.arg1;
    cpb_server_listen_once(s, eloop_idx);
    struct cpb_event new_ev = {
                               .itable = &cpb_server_event_handler,
                               .msg = {
                                .u.iip.argp = s,
                                .u.iip.arg1 = eloop_idx,
                                }
                              };
    struct cpb_eloop *eloop = s->elist->loops[eloop_idx].loop;
    cpb_assert_h(!!eloop, "");
    cpb_eloop_append_delayed(eloop, new_ev, CPB_HTTP_MIN_DELAY, 1);
}

struct cpb_error cpb_server_listen(struct cpb_server *s) {
    //TODO: error handling
    for (int eloop_idx=0; eloop_idx<s->elist->nloops; eloop_idx++) {
        struct cpb_event new_ev = {.itable = &cpb_server_event_handler,
                            .msg = {
                                .u = {
                                    .iip = {
                                        .argp = s,
                                        .arg1 = eloop_idx,
                                    }
                                }
                            }};
        cpb_server_ev_listen_loop(new_ev);
    }
    return cpb_make_error(CPB_OK);
}

void cpb_server_ev_destroy(struct cpb_event ev) {
    struct cpb_server *s = ev.msg.u.iip.argp;
}

struct cpb_event_handler_itable cpb_server_event_handler = {
    .handle = cpb_server_ev_listen_loop,
    .destroy = cpb_server_ev_destroy,
};

void cpb_server_deinit(struct cpb_server *s) {

    for (int i=0; i<CPB_SOCKET_MAX; i++) {
        cpb_http_multiplexer_deinit(&s->mp[i]);
    }
    cpb_http_server_config_deinit(s->cpb, &s->config);
    for (int i=0; i<s->elist->nloops; i++) {
        s->loop_data[i].listener->destroy(s->loop_data[i].listener);
        cpb_request_state_recycle_array_deinit(s->cpb, &s->loop_data[i].rq_cyc);
    }
    
    for (int i=0; i<s->n_loaded_modules; i++) {
        cpb_http_server_module_unload(s->cpb, s->loaded_modules[i].module, s->loaded_modules[i].dll_module_handle);
    }
    close(s->listen_socket_fd);
}

int cpb_server_set_request_handler(struct cpb_server *s, void (*handler)(struct cpb_request_state *rqstate, enum cpb_request_handler_reason reason)) {
    s->request_handler = handler;
    return CPB_OK;
}

int cpb_server_listener_switch(struct cpb_server *s, const char *listener_name) {
    struct cpb_server_listener_fdlist *fdlist = NULL;
    int rv = CPB_OK;

    struct cpb_server_listener *new_listeners[CPB_MAX_ELOOPS];
    int succeeded = 0;

    for (int eloop_idx=0; eloop_idx<s->elist->nloops; eloop_idx++) {
        struct cpb_eloop *eloop = s->elist->loops[eloop_idx].loop;
        struct cpb_server_listener *old_listener = s->loop_data[eloop_idx].listener;
        if (old_listener != NULL) {
            rv = old_listener->get_fds(old_listener, &fdlist);
            if (rv != CPB_OK) {
                goto rollback;
            }
        }
        
        struct cpb_server_listener *new_listener = NULL;
        if (strcasecmp(listener_name, "epoll") == 0) {        
            struct cpb_server_listener *epoll_listener = NULL;
            rv = cpb_server_listener_epoll_new(s, eloop, &epoll_listener);
            if (rv != CPB_OK) {
                cpb_server_listener_fdlist_destroy(s->cpb, fdlist);
                goto rollback;
            }
            new_listener = epoll_listener;
        }
        else if (strcasecmp(listener_name, "select") == 0) {
            struct cpb_server_listener *select_listener = NULL;
            rv = cpb_server_listener_select_new(s, eloop, &select_listener);
            if (rv != CPB_OK) {
                cpb_server_listener_fdlist_destroy(s->cpb, fdlist);
                goto rollback;
            }
            new_listener = select_listener;
        }
        else {
            cpb_server_listener_fdlist_destroy(s->cpb, fdlist);
            goto rollback;
        }
        
        if (fdlist) {
            for (int i=0; i<fdlist->len; i++) {
                new_listener->new_connection(new_listener, fdlist->fds[i]);
            }
        }
        
        if (fdlist) {
            cpb_server_listener_fdlist_destroy(s->cpb, fdlist);
        }
        new_listeners[succeeded] = new_listener;

        succeeded++;
    }
    cpb_assert_h(succeeded == s->elist->nloops, "");
    for (int eloop_idx=0; eloop_idx<s->elist->nloops; eloop_idx++) {
        if (s->loop_data[eloop_idx].listener)
            s->loop_data[eloop_idx].listener->destroy(s->loop_data[eloop_idx].listener);
        s->loop_data[eloop_idx].listener = new_listeners[eloop_idx];
    }    
    return CPB_OK;
rollback:
    for (int i=0; i<succeeded; i++) {
        new_listeners[i]->destroy(new_listeners[i]);
    }
    return rv;
}