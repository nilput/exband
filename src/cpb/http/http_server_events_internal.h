#ifndef CPB_HTTP_SERVER_EVENTS_INTERNAL
#define CPB_HTTP_SERVER_EVENTS_INTERNAL
#include "http_server_events.h"
#include "http_server_internal.h"
#include "http_request.h"
#include "http_response.h"
#include <unistd.h>
#include <errno.h>

void cpb_request_handle_socket_error(struct cpb_request_state *rqstate);
void cpb_request_handle_fatal_error(struct cpb_request_state *rqstate);
void cpb_request_handle_http_error(struct cpb_request_state *rqstate);

struct cpb_error cpb_response_write(struct cpb_request_state *rqstate);
struct cpb_error cpb_response_async_write(struct cpb_request_state *rqstate);
struct cpb_error cpb_request_async_read_from_client(struct cpb_request_state *rqstate);
struct cpb_error cpb_request_on_bytes_read(struct cpb_request_state *rqstate, int index, int nbytes);
struct cpb_error cpb_response_on_bytes_written(struct cpb_request_state *rqstate, int index, int nbytes);
void cpb_request_on_request_done(struct cpb_request_state *rqstate);
void cpb_request_on_response_done(struct cpb_request_state *rqstate);

enum cpb_event_http_cmd {
    CPB_HTTP_READ, /*ask it to read, .argp : rqstate*/
    CPB_HTTP_SEND, /*ask it to write, .argp : rqstate*/
    CPB_HTTP_DID_READ,  /*inform about async read result, .argp : rqstate*/
    CPB_HTTP_DID_WRITE, /*inform about async write result, .argp : rqstate*/
    CPB_HTTP_WRITE_IO_ERROR, /*inform about error during async read/write, .argp : rqstate*/
    CPB_HTTP_READ_IO_ERROR,
    CPB_HTTP_INPUT_BUFFER_FULL, /*inform about a full buffer during async read, .argp : rqstate*/
    CPB_HTTP_CLIENT_CLOSED, //.argp : rqstate
    CPB_HTTP_CANCEL, /*.argp: http server, .arg1: socket*/
};

static int cpb_event_http_init(struct cpb_server *s, struct cpb_event *ev, int cmd, void *object, int arg);

/*This is not the only source of bytes the request has, see also cpb_request_fork*/
static struct cpb_error cpb_request_read_from_client(struct cpb_request_state *rqstate) {
    int socket = rqstate->socket_fd;
    int avbytes = cpb_request_input_buffer_size(rqstate) - rqstate->input_buffer_len - 1;
    int nbytes;
    struct cpb_error err = {0};
    dp_register_event(__FUNCTION__);
    dp_register_event("read");
    nbytes = read(socket, rqstate->input_buffer + rqstate->input_buffer_len, avbytes);
    dp_end_event("read");
    if (nbytes < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            err = cpb_make_error(CPB_OK);
            goto ret;
        }
        else {
            err = cpb_make_error(CPB_READ_ERR);
            //fprintf(stderr, "READ ERROR");
            goto ret;
        }
    }
    else if (nbytes == 0) {
        struct cpb_event ev;
        if (avbytes == 0) {
            cpb_event_http_init(rqstate->server, &ev, CPB_HTTP_INPUT_BUFFER_FULL, rqstate, 0);
        }
        else {
            cpb_event_http_init(rqstate->server, &ev, CPB_HTTP_CLIENT_CLOSED, rqstate, 0);
        }
        //TODO error handling, also why not directly deal with the event
        cpb_eloop_append(rqstate->eloop, ev); 
        //fprintf(stderr, "EOF");
    }
    else {
        avbytes -= nbytes;
        int idx = rqstate->input_buffer_len;
        err = cpb_request_on_bytes_read(rqstate, idx, nbytes);
        if (err.error_code != CPB_OK)
            goto ret;
    }

    
    
    ret:
    dp_end_event(__FUNCTION__);
    return err;
}

static void on_http_read_sync(struct cpb_event ev) {
    struct cpb_request_state *rqstate = ev.msg.u.iip.argp;
    struct cpb_error err = cpb_make_error(CPB_OK);
    cpb_assert_h(rqstate->is_read_scheduled, "");
    RQSTATE_EVENT(stderr, "Handling CPB_HTTP_READ for rqstate %p\n", rqstate);
    rqstate->is_read_scheduled = 0;
    err = cpb_request_read_from_client(rqstate);
    if (err.error_code != CPB_OK) {
        cpb_request_handle_http_error(rqstate);
    }
    return err;
}

static void on_http_read_async(struct cpb_event ev) {
    struct cpb_request_state *rqstate = ev.msg.u.iip.argp;
    struct cpb_error err = cpb_make_error(CPB_OK);
    cpb_assert_h(rqstate->is_read_scheduled, "");
    RQSTATE_EVENT(stderr, "Handling CPB_HTTP_READ for rqstate %p\n", rqstate);
    cpb_request_async_read_from_client(rqstate);
    return err;
}


static void on_http_send_sync(struct cpb_event ev) {
    struct cpb_request_state *rqstate = ev.msg.u.iip.argp;
    struct cpb_error err = cpb_make_error(CPB_OK);
    cpb_assert_h(rqstate->is_send_scheduled, "");
    cpb_assert_h(rqstate->resp.state == CPB_HTTP_R_ST_SENDING, "HTTP_SEND scheduled on an unready response");
    RQSTATE_EVENT(stderr, "Handling CPB_HTTP_SEND for rqstate %p\n", rqstate);    

    err = cpb_response_write(rqstate);
    rqstate->is_send_scheduled = 0;
    if (err.error_code != CPB_OK) {
        cpb_request_handle_socket_error(rqstate);
        goto ret;
    }
    if (rqstate->resp.state == CPB_HTTP_R_ST_DONE) {
        cpb_request_on_response_done(rqstate);
    }

    ret:
    return err;
}


static void on_http_send_async(struct cpb_event ev) {
    struct cpb_request_state *rqstate = ev.msg.u.iip.argp;
    struct cpb_error err = cpb_make_error(CPB_OK);
    cpb_assert_h(rqstate->is_send_scheduled, "");
    cpb_assert_h(rqstate->resp.state == CPB_HTTP_R_ST_DONE, "HTTP_SEND scheduled on an unready response");
    RQSTATE_EVENT(stderr, "Handling CPB_HTTP_SEND for rqstate %p\n", rqstate);
    
    cpb_assert_h(rqstate->resp.state != CPB_HTTP_R_ST_DEAD, "");

    cpb_response_async_write(rqstate);

    ret:
    return err;
}

static void on_http_input_buffer_full(struct cpb_event ev) {
    struct cpb_request_state *rqstate = ev.msg.u.iip.argp;
    struct cpb_error err = cpb_make_error(CPB_OK);
    RQSTATE_EVENT(stderr, "INPUT BUFFER FULL For rqstate %p, sz: %d\n", rqstate, rqstate->input_buffer_cap);
    int rv;
    if ((rv = cpb_request_input_buffer_ensure_cap(rqstate, rqstate->input_buffer_cap * 2)) != CPB_OK) {
        cpb_request_handle_socket_error(rqstate);
        err = cpb_make_error(rv);
    }
    cpb_assert_h(rqstate->is_read_scheduled, "");
    rqstate->is_read_scheduled = 0;
    /*TODO: else reschedule?*/
}
static void on_http_did_read(struct cpb_event ev) {
    struct cpb_request_state *rqstate = ev.msg.u.iip.argp;
    struct cpb_error err = cpb_make_error(CPB_OK);
    RQSTATE_EVENT(stderr, "Handling async read notification of rqstate %p, %d bytes\n", rqstate, ev.msg.u.iip.arg1);
    cpb_assert_h(rqstate->is_read_scheduled, "");
    rqstate->is_read_scheduled = 0;
    
    cpb_assert_h(rqstate->istate != CPB_HTTP_I_ST_DEAD, ""); 
    int len  = ev.msg.u.iip.arg1;
    err = cpb_request_on_bytes_read(rqstate, rqstate->input_buffer_len, len);
    return err;
}

static void on_http_did_write(struct cpb_event ev) {
    struct cpb_request_state *rqstate = ev.msg.u.iip.argp;
    struct cpb_error err = cpb_make_error(CPB_OK);
    RQSTATE_EVENT(stderr, "Handling async write notification of rqstate %p, %d bytes\n", rqstate, ev.msg.u.iip.arg1);
    cpb_assert_h(rqstate->is_send_scheduled, "");
    rqstate->is_send_scheduled = 0;
    cpb_assert_h(rqstate->resp.state != CPB_HTTP_R_ST_DEAD, "");
    int len  = ev.msg.u.iip.arg1;
    err = cpb_response_on_bytes_written(rqstate, rqstate->resp.written_bytes, len);
    if (rqstate->resp.state == CPB_HTTP_R_ST_DONE) {
        cpb_request_on_response_done(rqstate);
    }
}

static void on_http_client_closed(struct cpb_event ev) {
    struct cpb_request_state *rqstate = ev.msg.u.iip.argp;
    /*TODO: This was closed by the client, check if it's valid*/
    /*For example if we are waiting for a request body and connection was closed prematurely*/
    RQSTATE_EVENT(stderr, "Client closed for rqstate %p, socket %d\n", rqstate, rqstate->socket_fd);
    cpb_server_cancel_requests(rqstate->server, rqstate->socket_fd);
}

static void on_http_cancel(struct cpb_event ev) {
    //being called on the socket/multiplexer not on a request
    struct cpb_server *s = ev.msg.u.iip.argp;
    int socket_fd = ev.msg.u.iip.arg1;
    struct cpb_http_multiplexer *mp = cpb_server_get_multiplexer_i(s, socket_fd);
    RQSTATE_EVENT(stderr, "Handling Cancel event for socket %d\n", socket_fd);
    cpb_assert_h(mp->state == CPB_MP_CANCELLING, "invalid mp state");
    cpb_assert_h(mp->socket_fd == socket_fd, "invalid mp state");
    struct cpb_request_state *next = mp->next_response;
    if (next != mp->creading && mp->creading) {
        cpb_assert_h(mp->creading->is_cancelled, "");
        cpb_request_on_request_done(mp->creading);
    }
    mp->creading = NULL;
    while (next) {
        cpb_assert_h(next->is_cancelled, "");
        struct cpb_request_state *tmp = next;
        next = next->next_rqstate;
        cpb_request_on_request_done(tmp);
    }
    mp->next_response = NULL;
    cpb_server_close_connection(s, socket_fd);
}

static void on_http_read_io_error(struct cpb_event ev) {
    struct cpb_request_state *rqstate = ev.msg.u.iip.argp;
    cpb_assert_h(rqstate->is_read_scheduled, "");
    rqstate->is_read_scheduled = 0;
    cpb_request_handle_socket_error(rqstate);
}


static void on_http_write_io_error(struct cpb_event ev) {
    struct cpb_request_state *rqstate = ev.msg.u.iip.argp;
    cpb_assert_h(rqstate->is_send_scheduled, "");
    rqstate->is_send_scheduled = 0;
    cpb_request_handle_socket_error(rqstate);
}


static int cpb_event_http_init(struct cpb_server *s, struct cpb_event *ev, int cmd, void *object, int arg) {
    switch (cmd) {
        case CPB_HTTP_READ:               ev->handle = s->on_read;                 break;
        case CPB_HTTP_SEND:               ev->handle = s->on_send;                 break;
        case CPB_HTTP_INPUT_BUFFER_FULL:  ev->handle = on_http_input_buffer_full;  break;
        case CPB_HTTP_CLIENT_CLOSED:      ev->handle = on_http_client_closed;      break;
        case CPB_HTTP_READ_IO_ERROR:      ev->handle = on_http_read_io_error;      break;
        case CPB_HTTP_WRITE_IO_ERROR:     ev->handle = on_http_write_io_error;     break;
        case CPB_HTTP_DID_READ:           ev->handle = on_http_did_read;           break;
        case CPB_HTTP_DID_WRITE:          ev->handle = on_http_did_write;          break;
        case CPB_HTTP_CANCEL:             ev->handle = on_http_cancel;             break;
        default: cpb_assert_h(0, "invalid cmd"); break;
    }
    ev->msg.u.iip.arg1 = arg;
    ev->msg.u.iip.arg2 = cmd;
    ev->msg.u.iip.argp = object;
    return CPB_OK;
}

static void cpb_server_on_read_available_i(struct cpb_server *s, struct cpb_http_multiplexer *m) {
    struct cpb_event ev;
    cpb_assert_h((!!m) && m->state == CPB_MP_ACTIVE, "");
    cpb_assert_h(!!m->creading, "");
    cpb_assert_h(!m->creading->is_read_scheduled, "");
    
    int flags = s->config.http_use_aio;
    cpb_event_http_init(s, &ev, CPB_HTTP_READ, m->creading, flags);
    m->creading->is_read_scheduled = 1;
    cpb_eloop_append(m->eloop, ev);
    RQSTATE_EVENT(stderr, "Scheduled rqstate %p to be read, because we found out "
                    "read is available for socket %d\n", m->creading, m->socket_fd);
    
}
static void cpb_server_on_write_available_i(struct cpb_server *s, struct cpb_http_multiplexer *m) {
    struct cpb_event ev;
    int flags = s->config.http_use_aio;
    cpb_event_http_init(s, &ev, CPB_HTTP_SEND, m->next_response, flags);
    m->next_response->is_send_scheduled = 1;
    cpb_eloop_append(m->eloop, ev);
    
}

#endif //CPB_HTTP_SERVER_EVENTS_INTERNAL