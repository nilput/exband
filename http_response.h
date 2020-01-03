#ifndef CPB_HTTP_RESPONSE_H
#define CPB_HTTP_RESPONSE_H

#include <stdbool.h>
#include "str.h"
#include "server_events.h"
#include "http_status.h"
#define HTTP_HEADERS_BUFFER_SIZE 2048
#define HTTP_OUTPUT_BUFFER_SIZE 6144
#define CPB_HTTP_RESPONSE_HEADER_MAX 32


enum cpb_http_response_state {
    CPB_HTTP_R_ST_INIT,
    CPB_HTTP_R_ST_READY_STATUS,
    CPB_HTTP_R_ST_READY_HEADERS,
    CPB_HTTP_R_ST_READY_BODY,
    CPB_HTTP_R_ST_SENDING,
    CPB_HTTP_R_ST_DONE
};
struct cpb_response_header {
    struct cpb_str key;
    struct cpb_str value;
};
struct cpb_response_headers {
    struct cpb_response_header headers[CPB_HTTP_RESPONSE_HEADER_MAX];
    int len;
};
struct cpb_response_state {
    struct cpb_request_state *req_state; //not owned, must outlive
    enum cpb_http_response_state state;
    int status_code;
    bool is_chunked; //currently not supported
    struct cpb_response_headers headers;
    int written_bytes;
    char headers_buff[HTTP_HEADERS_BUFFER_SIZE];
    int headers_buff_len;
    int output_buff_len;
    char output_buff[HTTP_OUTPUT_BUFFER_SIZE];
};

static int cpb_response_state_init(struct cpb_response_state *resp_state, struct cpb_request_state *req) {
    resp_state->req_state = req;
    resp_state->state = CPB_HTTP_R_ST_INIT;
    resp_state->is_chunked = 0;
    resp_state->status_code = 200;
    resp_state->output_buff[0] = 0;
    resp_state->output_buff_len = 0;
    resp_state->headers_buff[0] = 0;
    resp_state->headers_buff_len = 0;
    resp_state->written_bytes = 0;
    return CPB_OK;
}
//doesnt own name
static int cpb_response_get_header_index(struct cpb_response_state *rsp, const char *s, int len) {
    for (int i=0; i<rsp->headers.len; i++) {
        struct cpb_str *key = &rsp->headers.headers[i].key;
        if (cpb_strcasel_eq(key->str, key->len, s, len)) {
            return i;
        }
    }
    return -1;
}

static int cpb_response_done_headers(struct cpb_response_state *rsp) {
    return rsp->state != CPB_HTTP_R_ST_INIT &&
           rsp->state != CPB_HTTP_R_ST_READY_STATUS;
}

//Takes ownership of both name and value
int cpb_response_set_header(struct cpb_response_state *rsp, struct cpb_str *name, struct cpb_str *value);

//the bytes are copied to output buffer
static int cpb_response_append_body(struct cpb_response_state *rsp, char *s, int len) {
    int available_bytes = HTTP_OUTPUT_BUFFER_SIZE - rsp->output_buff_len - 1;
    if (len > available_bytes) {
        return CPB_OUT_OF_RANGE_ERR;
    }
    memcpy(rsp->output_buff + rsp->output_buff_len, s, len);
    rsp->output_buff_len += len;
    return CPB_OK;
}
static int cpb_response_prepare_status(struct cpb_response_state *rsp) {
    if (rsp->state != CPB_HTTP_R_ST_INIT) {
        return CPB_INVALID_STATE_ERR;
    }
    int nwritten = 0;
    int rv = cpb_write_status_code(rsp->headers_buff, HTTP_HEADERS_BUFFER_SIZE, &nwritten, rsp->status_code, 1, 1);
    if (rv != CPB_OK)
        return rv;
    rsp->headers_buff_len = nwritten;
    rsp->state = CPB_HTTP_R_ST_READY_STATUS;
    return CPB_OK;
}
static int cpb_response_prepare_headers(struct cpb_response_state *rsp) {
    if (rsp->state == CPB_HTTP_R_ST_INIT) {
        int rv = cpb_response_prepare_status(rsp);
        if (rv != CPB_OK)
            return rv;
    }
    if (rsp->state != CPB_HTTP_R_ST_READY_STATUS)
        return CPB_INVALID_STATE_ERR;

    for (int i=0; i<rsp->headers.len; i++) {
        struct cpb_response_header *h = &rsp->headers.headers[i];
        int to_write = 0;
        to_write += h->key.len;
        to_write += h->value.len;
        to_write += 2 /*': '*/ + 2 /*crlf*/;
        int size_left = HTTP_HEADERS_BUFFER_SIZE - rsp->headers_buff_len - 1;
        if (to_write > size_left) {
            return CPB_OUT_OF_RANGE_ERR;
        }
        memcpy(rsp->headers_buff + rsp->headers_buff_len, h->key.str, h->key.len);
        rsp->headers_buff_len += h->key.len;
        memcpy(rsp->headers_buff + rsp->headers_buff_len, ": ", 2);
        rsp->headers_buff_len += 2;
        memcpy(rsp->headers_buff + rsp->headers_buff_len, h->value.str, h->value.len);
        rsp->headers_buff_len += h->value.len;
        memcpy(rsp->headers_buff + rsp->headers_buff_len, "\r\n", 2);
        rsp->headers_buff_len += 2;
    }
    {
        int size_left = HTTP_HEADERS_BUFFER_SIZE - rsp->headers_buff_len - 1;
        if (size_left < 2) {
            return CPB_OUT_OF_RANGE_ERR;
        }
        memcpy(rsp->headers_buff + rsp->headers_buff_len, "\r\n", 2);
        rsp->headers_buff_len += 2;
    }

    rsp->state = CPB_HTTP_R_ST_READY_HEADERS;
    return CPB_OK;
}

int cpb_response_end(struct cpb_response_state *rsp);

int cpb_response_send(struct cpb_response_state *rsp);

#endif