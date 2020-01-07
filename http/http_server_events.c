#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include "../eloop.h"
#include "http_server.h"
#include "http_server_events.h"
#include "http_parse.h"

static void handle_http(struct cpb_event ev);
static void destroy_http(struct cpb_event ev);

struct cpb_event_handler_itable cpb_event_handler_http_itable = {
    .handle = handle_http,
    .destroy = destroy_http,
};

struct cpb_error read_from_client(struct cpb_request_state *rqstate, int socket) {
    int avbytes = HTTP_INPUT_BUFFER_SIZE - rqstate->input_buffer_len - 1;
    int nbytes;
    struct cpb_error err = {0};
    again:
    nbytes = read(socket, rqstate->input_buffer + rqstate->input_buffer_len, avbytes);
    if (nbytes < 0) {
        if (!(errno == EWOULDBLOCK || errno == EAGAIN)) {
            err = cpb_make_error(CPB_READ_ERR);
            fprintf(stderr, "READ ERROR");
        }
    }
    else if (nbytes == 0) {
        struct cpb_event ev;
        cpb_event_http_init(&ev, socket, CPB_HTTP_CLOSE, rqstate);
        //TODO error handling, also we can directly deal with the event because cache is hot
        cpb_eloop_append(rqstate->server->eloop, ev); 
        fprintf(stderr, "EOF");
    }
    else {
        {
            int scan_idx = rqstate->input_buffer_len - 3;
            scan_idx = scan_idx < 0 ? 0 : scan_idx;
            int scan_len = rqstate->input_buffer_len + nbytes;
            if (cpb_str_has_crlfcrlf(rqstate->input_buffer, scan_idx, scan_len)) {
                rqstate->istate = CPB_HTTP_I_ST_GOT_HEADERS;
                struct cpb_event ev;
                cpb_event_http_init(&ev, socket, CPB_HTTP_HANDLE_HEADERS, rqstate);
                //TODO error handling, also we can directly deal with the event because cache is hot
                cpb_eloop_append(rqstate->server->eloop, ev); 
            }
        }
        rqstate->input_buffer_len += nbytes;
        fprintf(stderr, "READ %d BYTES, TOTAL %d BYTES\n", nbytes, rqstate->input_buffer_len);
        avbytes -= nbytes;
        goto again;
    }
    return err;
}


static void cpb_request_handle_http_error(struct cpb_request_state *rqstate) {
    //TODO should terminate connection not whole server
    abort();
}
static void cpb_request_handle_fatal_error(struct cpb_request_state *rqstate) {
    //TODO should terminate connection not whole server
    abort();
}
static void cpb_request_call_handler(struct cpb_request_state *rqstate) {
    rqstate->server->request_handler(rqstate);
}

static void handle_http(struct cpb_event ev) {
    int socket_fd = ev.msg.arg1;
    int cmd  = ev.msg.arg2;
    struct cpb_request_state *rqstate = ev.msg.argp;
    if (cmd == CPB_HTTP_INIT || cmd == CPB_HTTP_READ)
        read_from_client(rqstate, socket_fd);
    else if (cmd == CPB_HTTP_CLOSE) {
        //Socket reached EOF
        cpb_server_close_connection(rqstate->server, socket_fd);
    }
    else if (cmd == CPB_HTTP_SEND && rqstate->resp.state != CPB_HTTP_R_ST_DONE) {
        int rv = cpb_response_send(&rqstate->resp);
        if (rv != CPB_OK) {
            cpb_request_handle_fatal_error(rqstate);
            return;
        }
    }
    else if (cmd == CPB_HTTP_HANDLE_HEADERS) {
        int rv = cpb_request_http_parse(rqstate);
        if (rv != CPB_OK) {
            cpb_request_handle_http_error(rqstate);
            return;
        }
        cpb_request_call_handler(rqstate);
    }
    return;
}
static void destroy_http(struct cpb_event ev) {
}

