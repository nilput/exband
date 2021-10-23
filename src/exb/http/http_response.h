#ifndef EXB_HTTP_RESPONSE_H
#define EXB_HTTP_RESPONSE_H

#include <stdbool.h>
#include "../exb_str.h"
#include "http_server_events.h"
#include "http_status.h"
#include "http_response_def.h"

/*
    The way we do responses is:
        Our goal is ending up with one buffer that has status.headers.body contigously
        What we do is: assume status + headers have x size, then start writing body at offset x
        then, at the end, once we know status + headers size write them before the body
*/

struct exb_request_state;
static struct exb_response_state *exb_request_get_response(struct exb_request_state *rqstate);

static int exb_response_state_init(struct exb_response_state *resp_state, struct exb_request_state *req, struct exb_evloop *evloop) {
    exb_assert_h(!!evloop, "");
    resp_state->state = EXB_HTTP_R_ST_INIT;
    //resp_state->is_chunked = 0;
    resp_state->status_code = 200;
    resp_state->written_bytes = 0;
    resp_state->headers.len = 0;

    resp_state->output_buffer = NULL;
    resp_state->output_buffer_cap = 0;
    resp_state->headers_bytes = 2;    /*final crlf*/
    resp_state->body_begin_index = 0;
    resp_state->status_begin_index = -1;
    resp_state->status_len = 0;
    resp_state->headers_begin_index = -1;
    resp_state->headers_len = 0;
    resp_state->body_begin_index = HTTP_HEADERS_BUFFER_INIT_SIZE;
    resp_state->body_len = 0;

    size_t buff_sz = 0;
    int rv = exb_evloop_alloc_buffer(evloop,
                                     HTTP_OUTPUT_BUFFER_INIT_SIZE + HTTP_HEADERS_BUFFER_INIT_SIZE,
                                     &resp_state->output_buffer,
                                     &buff_sz);
    resp_state->output_buffer_cap = buff_sz;
    exb_assert_h(resp_state->body_begin_index < resp_state->output_buffer_cap, "");
    if (rv != EXB_OK) {
        return rv;
    }

    return EXB_OK;
}

static int exb_response_state_deinit(struct exb_response_state *resp_state, struct exb *exb, struct exb_evloop *evloop) {
    resp_state->state = EXB_HTTP_R_ST_DEAD;
    for (int i=0; i<resp_state->headers.len; i++) {
        exb_str_deinit(exb, &resp_state->headers.headers[i].key);
        exb_str_deinit(exb, &resp_state->headers.headers[i].value);
    }
    exb_evloop_release_buffer(evloop, resp_state->output_buffer, resp_state->output_buffer_cap);
    return EXB_OK;
}


//doesnt own name
static int exb_response_get_header_index(struct exb_request_state *rqstate, const char *s, int len) {
    struct exb_response_state *rsp = exb_request_get_response(rqstate);
    
    for (int i=0; i<rsp->headers.len; i++) {
        struct exb_str *key = &rsp->headers.headers[i].key;
        if (exb_strcasel_eq(key->str, key->len, s, len)) {
            return i;
        }
    }
    return -1;
}

//Takes ownership of both name and value
int exb_response_set_header(struct exb_request_state *rqstate, struct exb_str *name, struct exb_str *value);
//Takes ownership of both name and value
int exb_response_add_header(struct exb_request_state *rqstate, struct exb_str *name, struct exb_str *value);

//doesnt take ownership
int exb_response_add_header_c(struct exb_request_state *rqstate, char *name, char *value);
//doesnt take ownership
int exb_response_set_header_c(struct exb_request_state *rqstate, char *name, char *value);

//doesnt own location
int exb_response_redirect_and_end(struct exb_request_state *rqstate, int status_code, const char *location);
int exb_response_end(struct exb_request_state *rqstate);

static size_t exb_response_body_available_bytes(struct exb_request_state *rqstate) {
    struct exb_response_state *rsp = exb_request_get_response(rqstate);
    return rsp->output_buffer_cap - rsp->body_begin_index - rsp->body_len;
}
static size_t exb_response_body_length(struct exb_request_state *rqstate) {
    struct exb_response_state *rsp = exb_request_get_response(rqstate);
    return rsp->body_len;
}
int exb_response_body_buffer_ensure(struct exb_request_state *rqstate, size_t cap);

//the bytes are copied to output buffer
static int exb_response_append_body_i(struct exb_request_state *rqstate, char *s, int len) {
    struct exb_response_state *rsp = exb_request_get_response(rqstate);
    
    int available_bytes = exb_response_body_available_bytes(rqstate);
    if (len > available_bytes) {
        int rv;
        if ((rv = exb_response_body_buffer_ensure(rqstate, exb_response_body_length(rqstate) + len)) != EXB_OK)
            return rv;
    }
    memcpy(rsp->output_buffer + rsp->body_begin_index + rsp->body_len, s, len);
    rsp->body_len += len;
    return EXB_OK;
}

/*
get a pointer for writing directly to the output buffer
*/
static int exb_response_append_body_buffer_ptr(struct exb_request_state *rqstate, char **buffer_out, size_t *max_len) {
    struct exb_response_state *rsp = exb_request_get_response(rqstate);
    *buffer_out = rsp->output_buffer + rsp->body_begin_index + rsp->body_len;
    *max_len    = exb_response_body_available_bytes(rqstate);
    return EXB_OK;
}

/*
When using exb_response_append_body_buffer_ptr() to write directly to the buffer
this must be called afterwards to indicate how many bytes were written
*/
static int exb_response_append_body_buffer_wrote(struct exb_request_state *rqstate, size_t len) {
    struct exb_response_state *rsp = exb_request_get_response(rqstate);
    rsp->body_len += len;
    return EXB_OK;
}

static int exb_response_append_body_cstr_i(struct exb_request_state *rqstate, char *s) {
    return exb_response_append_body_i(rqstate, s, strlen(s));
}

//the same functions with external linkage
int exb_response_append_body(struct exb_request_state *rqstate, char *s, int len);
int exb_response_append_body_cstr(struct exb_request_state *rqstate, char *s);


static int exb_response_prepare_headers(struct exb_request_state *rqstate, struct exb_evloop *evloop) {
    struct exb_response_state *rsp = exb_request_get_response(rqstate);

    if (rsp->state != EXB_HTTP_R_ST_INIT) {
        return EXB_INVALID_STATE_ERR;
    }
    exb_assert_h(rsp->headers_len == 0, "");
    exb_assert_h(rsp->headers_begin_index == -1, "");
    if ((rsp->headers_bytes + HTTP_STATUS_MAX_SZ) > rsp->body_begin_index) {
        //this is unlikely to happen
        char *new_buff;
        size_t new_buff_cap;
        int rv = exb_evloop_alloc_buffer(evloop, rsp->output_buffer_cap + rsp->headers_bytes + HTTP_STATUS_MAX_SZ, &new_buff, &new_buff_cap);
        if (rv != EXB_OK) {
            return rv;
        }
        int new_body_index = rsp->headers_bytes + HTTP_STATUS_MAX_SZ;
        memcpy(new_buff + new_body_index, rsp->output_buffer + rsp->body_begin_index, rsp->body_len);
        exb_evloop_release_buffer(evloop, rsp->output_buffer, rsp->output_buffer_cap);
        rsp->output_buffer = new_buff;
        rsp->output_buffer_cap = new_buff_cap;
        rsp->body_begin_index = new_body_index;
    }
    rsp->headers_begin_index = rsp->body_begin_index - rsp->headers_bytes;
    //prepare status
    exb_assert_h(rsp->headers_begin_index >= 0, "");
    int status_len = 0;
    char statusbuff[HTTP_STATUS_MAX_SZ];
    int rv = exb_write_status_code(statusbuff, HTTP_STATUS_MAX_SZ, &status_len, rsp->status_code, 1, 1);
    if (rv != EXB_OK)
        return rv;
    
    rsp->status_len = status_len;
    rsp->status_begin_index = rsp->headers_begin_index - rsp->status_len;
    memcpy(rsp->output_buffer + rsp->status_begin_index, statusbuff, status_len);
    //prepare headers
    for (int i=0; i<rsp->headers.len; i++) {
        struct exb_response_header *h = &rsp->headers.headers[i];
        char *key   = rsp->output_buffer + rsp->headers_begin_index + rsp->headers_len;
        char *colon = key + h->key.len;
        char *value = colon + 2;
        char *crlf  = value + h->value.len;
        memcpy(key,    h->key.str, h->key.len);
        memcpy(colon,  ": ",   2);
        memcpy(value,  h->value.str, h->value.len);
        memcpy(crlf,   "\r\n", 2);
        rsp->headers_len   += (crlf + 2) - key; //this header's length
    }
    
    memcpy(rsp->output_buffer + rsp->headers_begin_index + rsp->headers_len, "\r\n", 2);
    rsp->headers_len += 2;
    exb_assert_h(rsp->headers_bytes == rsp->headers_len, "headers size mismatch");

    rsp->state = EXB_HTTP_R_ST_READY_HEADERS;
    return EXB_OK;
}

static void exb_response_set_status_code(struct exb_request_state *rqstate, int status_code) {
    struct exb_response_state *rsp = exb_request_get_response(rqstate);
    rsp->status_code = status_code;
}

static int exb_response_return_error(struct exb_request_state *rqstate, int status_code, char *content) {
    exb_response_set_status_code(rqstate, status_code);
    exb_response_append_body_cstr(rqstate, content);
    return exb_response_end(rqstate);;
}

/*
called if an error occurs before or possibly after sending the headers,
this tries to recover from that
an example of this is read() successfully reading 100 bytes, then fails after that for some reason
the recovery from that should involve: padding the response with 0s, for each of the n bytes remaining
maybe trailer headers can be used for that?
*/
static int exb_response_on_error_mid_transfer(struct exb_request_state *rqstate) {
    /*TODO: BROKEN*/
    exb_response_set_status_code(rqstate, 500);
    exb_response_append_body_cstr(rqstate, "transfer error");
    return exb_response_end(rqstate);;
}



#endif
