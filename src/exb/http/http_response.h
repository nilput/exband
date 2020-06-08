#ifndef EXB_HTTP_RESPONSE_H
#define EXB_HTTP_RESPONSE_H

#include <stdbool.h>
#include "../exb_str.h"
#include "http_server_events.h"
#include "http_status.h"
#define HTTP_HEADERS_BUFFER_INIT_SIZE 512
#define HTTP_OUTPUT_BUFFER_INIT_SIZE 2048
#define EXB_HTTP_RESPONSE_HEADER_MAX 16
#define HTTP_STATUS_MAX_SZ 64

struct exb_request_state;
static struct exb_response_state *exb_request_get_response(struct exb_request_state *rqstate);

enum exb_http_response_state {
    EXB_HTTP_R_ST_INIT,
    EXB_HTTP_R_ST_READY_HEADERS,
    EXB_HTTP_R_ST_READY_BODY,
    EXB_HTTP_R_ST_SENDING,
    EXB_HTTP_R_ST_DONE,
    EXB_HTTP_R_ST_DEAD
};
struct exb_response_header {
    struct exb_str key;
    struct exb_str value;
};
struct exb_response_headers {
    struct exb_response_header headers[EXB_HTTP_RESPONSE_HEADER_MAX];
    int len;
};
struct exb_response_state {
    char *output_buffer; //layout: [space?] STATUS . HEADERS . BODY
    int output_buffer_cap;
    enum exb_http_response_state state;
    
    //bool is_chunked; //currently not supported
    int written_bytes;
    
    int headers_bytes; //whenever we set a header we accumulate it length + crlf to this member
                       //inititalized to 2 (because there's a final crlf after all headers)
    int status_begin_index;  //inititalized to -1
    int status_len;          //inititalized to -1
    int headers_begin_index; //inititalized to -1
    int headers_len;         //inititalized to -1
    int body_begin_index;    //inititalized to HEADERS_BUFFER_INIT_SIZE
    int body_len;            //inititalized to 0

    int status_code;
    struct exb_response_headers headers;
};
/*
    The way we do responses is:
        Our goal is ending up with one buffer that has status.headers.body contigously
        What we do is: assume status + headers have x size, then start writing body at offset x
        then, at the end, once we know status + headers size write them before the body
*/

static int exb_response_state_init(struct exb_response_state *resp_state, struct exb_request_state *req, struct exb_eloop *eloop) {
    exb_assert_h(!!eloop, "");
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

    struct exb_error err = exb_eloop_alloc_buffer(eloop,
                                HTTP_OUTPUT_BUFFER_INIT_SIZE + HTTP_HEADERS_BUFFER_INIT_SIZE,
                                &resp_state->output_buffer,
                                &resp_state->output_buffer_cap);
    exb_assert_h(resp_state->body_begin_index < resp_state->output_buffer_cap, "");
    if (err.error_code) {
        return err.error_code;
    }

    return EXB_OK;
}

static int exb_response_state_deinit(struct exb_response_state *resp_state, struct exb *exb, struct exb_eloop *eloop) {
    resp_state->state = EXB_HTTP_R_ST_DEAD;
    for (int i=0; i<resp_state->headers.len; i++) {
        exb_str_deinit(exb, &resp_state->headers.headers[i].key);
        exb_str_deinit(exb, &resp_state->headers.headers[i].value);
    }
    exb_eloop_release_buffer(eloop, resp_state->output_buffer, resp_state->output_buffer_cap);
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
static int exb_response_append_body(struct exb_request_state *rqstate, char *s, int len) {
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

int exb_response_append_body_cstr(struct exb_request_state *rqstate, char *s);

static int exb_response_prepare_headers(struct exb_request_state *rqstate, struct exb_eloop *eloop) {
    struct exb_response_state *rsp = exb_request_get_response(rqstate);

    if (rsp->state != EXB_HTTP_R_ST_INIT) {
        return EXB_INVALID_STATE_ERR;
    }
    exb_assert_h(rsp->headers_len == 0, "");
    exb_assert_h(rsp->headers_begin_index == -1, "");
    if ((rsp->headers_bytes + HTTP_STATUS_MAX_SZ) > rsp->body_begin_index) {
        //this is unlikely to happen
        char *new_buff;
        int new_buff_cap;
        struct exb_error err = exb_eloop_alloc_buffer(eloop, rsp->output_buffer_cap + rsp->headers_bytes + HTTP_STATUS_MAX_SZ, &new_buff, &new_buff_cap);
        if (err.error_code) {
            return err.error_code;
        }
        int new_body_index = rsp->headers_bytes + HTTP_STATUS_MAX_SZ;
        memcpy(new_buff + new_body_index, rsp->output_buffer + rsp->body_begin_index, rsp->body_len);
        exb_eloop_release_buffer(eloop, rsp->output_buffer, rsp->output_buffer_cap);
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

//doesnt own location
int exb_response_redirect_and_end(struct exb_request_state *rqstate, int status_code, const char *location);
int exb_response_end(struct exb_request_state *rqstate);

#endif
