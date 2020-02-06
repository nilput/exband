#ifndef CPB_HTTP_RESPONSE_H
#define CPB_HTTP_RESPONSE_H

#include <stdbool.h>
#include "../cpb_str.h"
#include "http_server_events.h"
#include "http_status.h"
#define HTTP_HEADERS_BUFFER_INIT_SIZE 512
#define HTTP_OUTPUT_BUFFER_INIT_SIZE 2048
#define CPB_HTTP_RESPONSE_HEADER_MAX 32
#define HTTP_STATUS_MAX_SZ 128

struct cpb_request_state;
static struct cpb_response_state *cpb_request_get_response(struct cpb_request_state *rqstate);

enum cpb_http_response_state {
    CPB_HTTP_R_ST_INIT,
    CPB_HTTP_R_ST_READY_HEADERS,
    CPB_HTTP_R_ST_READY_BODY,
    CPB_HTTP_R_ST_SENDING,
    CPB_HTTP_R_ST_DONE,
    CPB_HTTP_R_ST_DEAD
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
    char *output_buffer; //layout: [space?] STATUS . HEADERS . BODY
    int output_buffer_cap;
    enum cpb_http_response_state state;
    
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
    struct cpb_response_headers headers;
};
/*
    The way we do responses is:
        Our goal is ending up with one buffer that has status.headers.body contigously
        What we do is: assume status + headers have x size, then start writing body at offset x
        then, at the end, once we know status + headers size write them before the body
*/

static int cpb_response_state_init(struct cpb_response_state *resp_state, struct cpb_request_state *req, struct cpb_eloop *eloop) {
    cpb_assert_h(!!eloop, "");
    resp_state->state = CPB_HTTP_R_ST_INIT;
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

    struct cpb_error err = cpb_eloop_alloc_buffer(eloop,
                                HTTP_OUTPUT_BUFFER_INIT_SIZE + HTTP_HEADERS_BUFFER_INIT_SIZE,
                                &resp_state->output_buffer,
                                &resp_state->output_buffer_cap);
    cpb_assert_h(resp_state->body_begin_index < resp_state->output_buffer_cap, "");
    if (err.error_code) {
        return err.error_code;
    }

    return CPB_OK;
}

static int cpb_response_state_deinit(struct cpb_response_state *resp_state, struct cpb *cpb, struct cpb_eloop *eloop) {
    resp_state->state = CPB_HTTP_R_ST_DEAD;
    for (int i=0; i<resp_state->headers.len; i++) {
        cpb_str_deinit(cpb, &resp_state->headers.headers[i].key);
        cpb_str_deinit(cpb, &resp_state->headers.headers[i].value);
    }
    cpb_eloop_release_buffer(eloop, resp_state->output_buffer, resp_state->output_buffer_cap);
    return CPB_OK;
}


//doesnt own name
static int cpb_response_get_header_index(struct cpb_request_state *rqstate, const char *s, int len) {
    struct cpb_response_state *rsp = cpb_request_get_response(rqstate);
    
    for (int i=0; i<rsp->headers.len; i++) {
        struct cpb_str *key = &rsp->headers.headers[i].key;
        if (cpb_strcasel_eq(key->str, key->len, s, len)) {
            return i;
        }
    }
    return -1;
}

//Takes ownership of both name and value
int cpb_response_set_header(struct cpb_request_state *rqstate, struct cpb_str *name, struct cpb_str *value);
//Takes ownership of both name and value
int cpb_response_add_header(struct cpb_request_state *rqstate, struct cpb_str *name, struct cpb_str *value);
static size_t cpb_response_body_available_bytes(struct cpb_request_state *rqstate) {
    struct cpb_response_state *rsp = cpb_request_get_response(rqstate);
    return rsp->output_buffer_cap - rsp->body_begin_index - rsp->body_len;
}

int cpb_response_body_buffer_ensure(struct cpb_request_state *rqstate, size_t cap);
//the bytes are copied to output buffer
static int cpb_response_append_body(struct cpb_request_state *rqstate, char *s, int len) {
    struct cpb_response_state *rsp = cpb_request_get_response(rqstate);
    
    int available_bytes = cpb_response_body_available_bytes(rqstate);
    if (len > available_bytes) {
        int rv;
        size_t new_cap = rsp->output_buffer_cap;
        cpb_assert_h(new_cap > 0, "");
        while (new_cap < (len - available_bytes))
            new_cap *= 2;
        if ((rv = cpb_response_body_buffer_ensure(rqstate, new_cap)) != CPB_OK)
            return rv;
    }
    memcpy(rsp->output_buffer + rsp->body_begin_index + rsp->body_len, s, len);
    rsp->body_len += len;
    return CPB_OK;
}
static int cpb_response_append_body_cstr(struct cpb_request_state *rqstate, char *s) {
    return cpb_response_append_body(rqstate, s, strlen(s));
}


static int cpb_response_prepare_headers(struct cpb_request_state *rqstate, struct cpb_eloop *eloop) {
    struct cpb_response_state *rsp = cpb_request_get_response(rqstate);

    if (rsp->state != CPB_HTTP_R_ST_INIT) {
        return CPB_INVALID_STATE_ERR;
    }
    cpb_assert_h(rsp->headers_len == 0, "");
    cpb_assert_h(rsp->headers_begin_index == -1, "");
    if ((rsp->headers_bytes + HTTP_STATUS_MAX_SZ) > rsp->body_begin_index) {
        //this should be unlikely to happen
        char *new_buff;
        int new_buff_cap;
        struct cpb_error err = cpb_eloop_alloc_buffer(eloop, rsp->output_buffer_cap + rsp->headers_bytes + HTTP_STATUS_MAX_SZ, &new_buff, &new_buff_cap);
        if (err.error_code) {
            return err.error_code;
        }
        int new_body_index = rsp->headers_bytes + HTTP_STATUS_MAX_SZ;
        memcpy(new_buff + new_body_index, rsp->output_buffer + rsp->body_begin_index, rsp->body_len);
        cpb_eloop_release_buffer(eloop, rsp->output_buffer, rsp->output_buffer_cap);
        rsp->output_buffer = new_buff;
        rsp->output_buffer_cap = new_buff_cap;
        rsp->body_begin_index = new_body_index;
    }
    rsp->headers_begin_index = rsp->body_begin_index - rsp->headers_bytes;
    //prepare status
    cpb_assert_h(rsp->headers_begin_index >= 0, "");
    int status_len = 0;
    char statusbuff[HTTP_STATUS_MAX_SZ];
    int rv = cpb_write_status_code(statusbuff, HTTP_STATUS_MAX_SZ, &status_len, rsp->status_code, 1, 1);
    if (rv != CPB_OK)
        return rv;
    
    rsp->status_len = status_len;
    rsp->status_begin_index = rsp->headers_begin_index - rsp->status_len;
    memcpy(rsp->output_buffer + rsp->status_begin_index, statusbuff, status_len);
    //prepare headers
    for (int i=0; i<rsp->headers.len; i++) {
        struct cpb_response_header *h = &rsp->headers.headers[i];
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
    cpb_assert_h(rsp->headers_bytes == rsp->headers_len, "headers size mismatch");

    rsp->state = CPB_HTTP_R_ST_READY_HEADERS;
    return CPB_OK;
}

static void cpb_response_set_status_code(struct cpb_request_state *rqstate, int status_code) {
    struct cpb_response_state *rsp = cpb_request_get_response(rqstate);
    rsp->status_code = status_code;
}

//doesnt own location
int cpb_response_redirect_and_end(struct cpb_request_state *rqstate, int status_code, const char *location);
int cpb_response_end(struct cpb_request_state *rqstate);

#endif
