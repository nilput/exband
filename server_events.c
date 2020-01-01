#include "eloop.h"
#include "server.h"
#include "server_events.h"
#include "http_parse.h"
#include <errno.h>
#include <stdio.h>
#include <unistd.h>

static void handle_http(struct cpb_event ev);
static void destroy_http(struct cpb_event ev);

struct cpb_event_handler_itable cpb_event_handler_http_itable = {
    .handle = handle_http,
    .destroy = destroy_http,
};

struct cpb_error read_from_client(struct cpb_request_state *rstate, int socket) {
    int avbytes = HTTP_INPUT_BUFFER_SIZE - rstate->input_buffer_len - 1;
    int nbytes;
    struct cpb_error err = {0};
    again:
    nbytes = read(socket, rstate->input_buffer + rstate->input_buffer_len, avbytes);
    if (nbytes < 0) {
        if (!(errno == EWOULDBLOCK || errno == EAGAIN)) {
            err = cpb_make_error(CPB_READ_ERR);
            fprintf(stderr, "READ ERROR");
        }
    }
    else if (nbytes == 0) {
        struct cpb_event ev;
        
        cpb_event_http_init(&ev, socket, CPB_HTTP_CLOSE, rstate);
        //TODO error handling, also we can directly deal with the event because cache is hot
        cpb_eloop_append(rstate->server->eloop, ev); 
        fprintf(stderr, "EOF");
    }
    else {
        {
            int scan_idx = rstate->input_buffer_len - 3;
            scan_idx = scan_idx < 0 ? 0 : scan_idx;
            int scan_len = rstate->input_buffer_len + nbytes;
            if (cpb_str_has_crlfcrlf(rstate->input_buffer, scan_idx, scan_len)) {
                rstate->istate = CPB_HTTP_I_ST_GOT_HEADERS;
                struct cpb_event ev;
                cpb_event_http_init(&ev, socket, CPB_HTTP_HANDLE_HEADERS, rstate);
                //TODO error handling, also we can directly deal with the event because cache is hot
                cpb_eloop_append(rstate->server->eloop, ev); 
            }
        }
        rstate->input_buffer_len += nbytes;
        fprintf(stderr, "READ %d BYTES, TOTAL %d BYTES\n", nbytes, rstate->input_buffer_len);
        avbytes -= nbytes;
        goto again;
    }
    return err;
}

static void handle_http(struct cpb_event ev) {
    int socket_fd = ev.msg.arg1;
    int cmd  = ev.msg.arg2;
    struct cpb_request_state *rstate = ev.msg.argp;
    if (cmd == CPB_HTTP_INIT || cmd == CPB_HTTP_READ)
        read_from_client(rstate, socket_fd);
    if (cmd == CPB_HTTP_CLOSE || cmd == CPB_HTTP_HANDLE_HEADERS) {
        cpb_request_http_parse(rstate);
    }
    return;
}
static void destroy_http(struct cpb_event ev) {
}

