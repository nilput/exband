
#include <unistd.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <fcntl.h> /* nonblocking sockets */
#include <errno.h> 

#include "../cpb_errors.h"
#include "http_server.h"
#include "http_server_events.h"
#include "http_parse.h"
#include "http_request.h"
#include "http_socket_multiplexer.h"

#include "http_server_listener_select.h"

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
    cpb_assert_h(rqstate->server && rqstate->server->handler_module, "");
    //TODO: dont ignore, also too many indirections that can be easily avoided!
    struct cpb_server *s = rqstate->server;
    int ignored = s->handler_module->handle_request(s->handler_module, rqstate, reason);
}


struct cpb_error cpb_server_init_with_config(struct cpb_server *s, struct cpb *cpb_ref, struct cpb_eloop *eloop, struct cpb_http_server_config config) {
    struct cpb_error err = {0};
    s->cpb = cpb_ref;
    s->eloop = eloop;
    s->request_handler   = default_handler;
    s->handler_module    = NULL;
    s->dll_module_handle = NULL;

    s->config = config;
    for (int i=0; i<CPB_SOCKET_MAX; i++) {
        cpb_http_multiplexer_init(&s->mp[i], NULL);
    }
    /* Create the socket and set it up to accept connections. */
    struct cpb_or_socket or_socket = make_socket(s->config.http_listen_port);
    if (or_socket.error.error_code) {
        err = cpb_prop_error(or_socket.error);
        goto err0;
    }
    else if (or_socket.value >= CPB_SOCKET_MAX) {
        err = cpb_make_error(CPB_OUT_OF_RANGE_ERR);
        goto err0;
    }
    
    int socket_fd = or_socket.value;
    s->listen_socket_fd = socket_fd;

    if (listen(s->listen_socket_fd, LISTEN_BACKLOG) < 0) { 
        err = cpb_make_error(CPB_LISTEN_ERR);
        goto err1;
    }

    int rv = cpb_server_listener_select_new(s, &s->listener);
    if (rv != CPB_OK) {
        err = cpb_make_error(rv);
        goto err1;
    }

    if (config.http_handler_module.len > 0) {
        int error = cpb_http_handler_module_load(cpb_ref, s, config.http_handler_module.str, config.http_handler_module_args.str, &s->handler_module, &s->dll_module_handle);
        if (error != CPB_OK) {
            err = cpb_make_error(CPB_MODULE_LOAD_ERROR);
            goto err2;
        }
        s->request_handler = module_handler;
    }

    return err;
    /*
    err3:
    if (s->handler_module) {
        cpb_http_handler_module_unload(s->cpb, s->handler_module, s->dll_module_handle);
    }
    */
err2:
    s->listener->destroy(s, s->listener);
err1:
    close(s->listen_socket_fd);
err0:
    for (int i=0; i<CPB_SOCKET_MAX; i++) {
        cpb_http_multiplexer_deinit(&s->mp[i]);
    }
    return err;
}
struct cpb_error cpb_server_init(struct cpb_server *s, struct cpb *cpb_ref, struct cpb_eloop *eloop, int port) {
    struct cpb_http_server_config config = cpb_http_server_config_default(cpb_ref);
    config.http_listen_port = port;
    struct cpb_error err = cpb_server_init_with_config(s, cpb_ref, eloop, config);
    if (err.error_code != CPB_OK) {
        cpb_http_server_config_deinit(cpb_ref, &config);
    }
    return err;
}


struct cpb_http_multiplexer *cpb_server_get_multiplexer(struct cpb_server *s, int socket_fd) 
{
    if (socket_fd > CPB_SOCKET_MAX)
        return NULL;
    cpb_assert_h(socket_fd >= 0, ""), "invalid socket no";
    return s->mp + socket_fd;
}

struct cpb_request_state *cpb_server_new_rqstate(struct cpb_server *server, struct cpb_eloop *eloop, int socket_fd) {
    dp_register_event(__FUNCTION__);
    void *p = cpb_malloc(server->cpb, sizeof(struct cpb_request_state));
    if (!p) {
        dp_end_event(__FUNCTION__);
        return NULL;
    }
    struct cpb_request_state *st = p;
    cpb_request_state_init(st, eloop, server->cpb, server, socket_fd);
    dp_end_event(__FUNCTION__);
    return st;
}
void cpb_server_destroy_rqstate(struct cpb_server *server, struct cpb_request_state *rqstate) {
    dp_register_event(__FUNCTION__);
    cpb_request_state_deinit(rqstate, server->cpb);
    cpb_free(server->cpb, rqstate);
    dp_end_event(__FUNCTION__);
}

struct cpb_eloop * cpb_server_get_any_eloop(struct cpb_server *s) {
    return s->eloop;
}

int cpb_server_init_multiplexer(struct cpb_server *s, int socket_fd, struct sockaddr_in clientname) {
    
    fcntl(socket_fd, F_SETFL, O_NONBLOCK); /* Change the socket into non-blocking state */
    struct cpb_http_multiplexer *mp = cpb_server_get_multiplexer(s, socket_fd);
    if (mp == NULL)
        return CPB_SOCKET_ERR;

    struct cpb_eloop *eloop =  cpb_server_get_any_eloop(s);
    cpb_assert_h(!!eloop, "");
    cpb_http_multiplexer_init(mp, eloop);

    mp->state = CPB_MP_ACTIVE;
    
    mp->clientname = clientname;
    struct cpb_request_state *rqstate = cpb_server_new_rqstate(s, mp->eloop, socket_fd);
    fprintf(stderr,
            "Server: connection from host %s, port %hu.\n",
            inet_ntoa (clientname.sin_addr),
            ntohs (clientname.sin_port));
    
    s->listener->new_connection(s, s->listener, socket_fd);
    
    mp->creading = rqstate;
    cpb_http_multiplexer_queue_response(mp, rqstate);
    
    struct cpb_event ev;
    rqstate->is_read_scheduled = 1;
    cpb_event_http_init(&ev, CPB_HTTP_INIT, rqstate, 0);
    cpb_eloop_append(mp->eloop, ev);
    return CPB_OK;
}

void cpb_server_cancel_requests(struct cpb_server *s, int socket_fd) {
    /*TODO
    deschedule all requests and destroy them
    */
}
void cpb_server_close_connection(struct cpb_server *s, int socket_fd) {
    close(socket_fd);
    s->listener->close_connection(s, s->listener, socket_fd);
    cpb_http_multiplexer_deinit(s->mp + socket_fd);
}

void cpb_server_on_read_available(struct cpb_server *s, struct cpb_http_multiplexer *m) {
    struct cpb_event ev;
    cpb_assert_h((!!m) && m->state == CPB_MP_ACTIVE, "");
    cpb_assert_h(!!m->creading, "");
    cpb_assert_h(!m->creading->is_read_scheduled, "");
    m->creading->is_read_scheduled = 1;
    cpb_event_http_init(&ev, CPB_HTTP_READ, m->creading, 0);
    cpb_eloop_append(m->eloop, ev);
}
void cpb_server_on_write_available(struct cpb_server *s, struct cpb_http_multiplexer *m) {
    struct cpb_event ev;
    cpb_event_http_init(&ev, CPB_HTTP_SEND, m->next_response, 0);
    cpb_eloop_append(m->eloop, ev);
    m->next_response->is_send_scheduled = 1;
}

struct cpb_error cpb_server_listen_once(struct cpb_server *s) {
    struct cpb_error err = {0};
    dp_register_event(__FUNCTION__);
    cpb_assert_h(!!s->listener, "");
    

    s->listener->listen(s, s->listener);
    
ret:
    dp_end_event(__FUNCTION__);
    return err;
}

struct cpb_event_handler_itable cpb_server_event_handler;
void cpb_server_ev_listen_loop(struct cpb_event ev) {
    struct cpb_server *s = ev.msg.u.iip.argp;
    cpb_server_listen_once(s);
    struct cpb_event new_ev = {
                               .itable = &cpb_server_event_handler,
                               .msg = {
                                .u.iip.argp = s
                                }
                              };
    struct cpb_eloop *eloop = cpb_server_get_any_eloop(s);
    cpb_assert_h(!!eloop, "");
    cpb_eloop_append_delayed(eloop, new_ev, CPB_HTTP_MIN_DELAY, 1);
}

struct cpb_error cpb_server_listen(struct cpb_server *s) {
    struct cpb_event new_ev = {.itable = &cpb_server_event_handler,
                           .msg = {
                               .u = {
                                .iip = {
                                        .argp = s
                                }
                               }
                           }};
    cpb_server_ev_listen_loop(new_ev);
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
    s->listener->destroy(s, s->listener);
    if (s->handler_module) {
        cpb_http_handler_module_unload(s->cpb, s->handler_module, s->dll_module_handle);
    }
    close(s->listen_socket_fd);
}

int cpb_server_set_request_handler(struct cpb_server *s, void (*handler)(struct cpb_request_state *rqstate, enum cpb_request_handler_reason reason)) {
    s->request_handler = handler;
    return CPB_OK;
}