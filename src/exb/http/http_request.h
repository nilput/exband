#ifndef EXB_HTTP_REQUEST_H
#define EXB_HTTP_REQUEST_H
#include "../exb_str.h"
#include "http_response.h"
#include "http_request_handler.h"
#include "http_request_def.h"
#include "http_server_def.h"
#include "http_server.h"
#include <stdbool.h>


//boolean
static int exb_request_http_version_eq(struct exb_request_state *rqstate, int major, int minor) {
    return rqstate->http_major == major && rqstate->http_minor == minor;
}

void exb_request_repr(struct exb_request_state *rqstate);

static int exb_request_input_buffer_size(struct exb_request_state *rqstate) {
    return rqstate->input_buffer_cap;
}

static struct exb_msg *exb_request_get_userdata(struct exb_request_state *rqstate) {
    return &rqstate->userdata;
}

static int exb_request_get_evloop_index(struct exb_request_state *rqstate) {
    return exb_server_evloop_id(rqstate->server, rqstate->evloop);
}



static int exb_request_input_buffer_ensure_cap(struct exb_request_state *rqstate, size_t capacity) {
    if (capacity > rqstate->input_buffer_cap) {
        int rv = exb_evloop_realloc_buffer(rqstate->evloop,
                                           rqstate->input_buffer,
                                           capacity,
                                           &rqstate->input_buffer,
                                           &rqstate->input_buffer_cap);
        return rv;
    }
    return EXB_OK;
}

static int exb_request_state_init(struct exb_request_state *rqstate, struct exb_evloop *evloop, struct exb *exb, struct exb_server *s, int socket_fd) {
    rqstate->request_handler = s->request_handler;
    rqstate->rqh_state       = s->request_handler_state;
    rqstate->is_chunked = 0;
    rqstate->evloop = evloop;
    rqstate->is_persistent = 0;
    rqstate->is_read_scheduled = 0;
    rqstate->is_send_scheduled = 0;
    rqstate->is_cancelled = 0;
    rqstate->is_forked = 0;

    rqstate->socket_fd = socket_fd;
    rqstate->server = s;
    rqstate->input_buffer = NULL;
    rqstate->input_buffer_len = 0;
    rqstate->input_buffer_cap = 0;
    rqstate->bytes_read = 0;
    rqstate->parse_chunk_cursor = 0;
    rqstate->next_request_cursor = -1;
    rqstate->istate = EXB_HTTP_I_ST_INIT;
    rqstate->pstate = EXB_HTTP_P_ST_INIT;
    rqstate->body_handling = EXB_HTTP_B_DISCARD;
    rqstate->headers.h_connection_idx        = -1;
    rqstate->headers.h_content_length_idx    = -1;
    rqstate->headers.h_content_type_idx      = -1;
    rqstate->headers.h_transfer_encoding_idx = -1;
    rqstate->headers.h_host_idx = -1;
    rqstate->headers.len = 0;
    rqstate->next_rqstate = NULL;
    size_t buff_cap = 0;
    int rv = exb_evloop_alloc_buffer(evloop,
                                     HTTP_INPUT_BUFFER_INITIAL_SIZE,
                                     &rqstate->input_buffer,
                                     &buff_cap);
    rqstate->input_buffer_cap = buff_cap;
    if (rv != EXB_OK) {
        return rv;
    }

    exb_str_init_empty(&rqstate->body_decoded);
    return exb_response_state_init(&rqstate->resp, rqstate, evloop);
}

static int exb_request_state_change_handler(struct exb_request_state *rqstate, exb_request_handler_func func, void *rqh_state) {
    rqstate->rqh_state =rqh_state;
    rqstate->request_handler = func;
    return EXB_OK;
}

static int exb_request_body_bytes_read(struct exb_request_state *rqstate) {
    return rqstate->bytes_read - rqstate->body_s.index;
}

static struct exb *exb_request_get_exb_ref(struct exb_request_state *rqstate) {
    return rqstate->server->exb;
}

static int exb_request_get_path_slice(struct exb_request_state *rqstate, char **out, struct exb_str_slice *slice_out) {
    *slice_out = rqstate->path_s;
    *out = rqstate->input_buffer;
    return EXB_OK;
}
//must be freed with exb_str_deinit
//assumes str_out is uninitialized
static int exb_request_get_path_copy(struct exb_request_state *rqstate, struct exb_str *str_out) {
    exb_str_init_empty(str_out);
    int rv = exb_str_slice_to_copied_str(rqstate->server->exb, rqstate->path_s, rqstate->input_buffer, str_out);
    return rv;
}

static void exb_request_state_deinit(struct exb_request_state *rqstate, struct exb *exb) {
    exb_response_state_deinit(&rqstate->resp, exb, rqstate->evloop);
    exb_evloop_release_buffer(rqstate->evloop, rqstate->input_buffer, rqstate->input_buffer_cap);
    exb_str_deinit(exb, &rqstate->body_decoded);
    rqstate->istate = EXB_HTTP_I_ST_DEAD;
}

static int exb_request_has_body(struct exb_request_state *rqstate) {
    return (rqstate->headers.h_content_length_idx != -1) || (rqstate->headers.h_transfer_encoding_idx != -1);
}
static int exb_request_is_chunked_body_complete(struct exb_request_state *rqstate) {
    return rqstate->pstate == EXB_HTTP_P_ST_DONE;
}

static int exb_request_http_check_validity(struct exb_request_state *rqstate) {
    int has_body = exb_request_has_body(rqstate);
    /*Conditions we are checking for:*/
    /*  Body must not be present in Trace*/
    /*  in Requests with bodies, Content-Length or Chunked encoding must be used, otherwise we can't respond*/
    if (has_body && rqstate->method == EXB_HTTP_M_TRACE)
        return EXB_HTTP_ERROR;
    return EXB_OK;
}

static struct exb_response_state *exb_request_get_response(struct exb_request_state *rqstate) {
    return &rqstate->resp;
}


#endif// EXB_HTTP_REQUEST_H
