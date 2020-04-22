#ifndef EXB_HTTP_REQUEST_H
#define EXB_HTTP_REQUEST_H
#include "../exb_str.h"
#include "http_response.h"
#include <stdbool.h>

#define EXB_HTTP_HEADER_MAX 32
#define HTTP_INPUT_BUFFER_INITIAL_SIZE 2048
enum EXB_HTTP_METHOD {
    EXB_HTTP_M_HEAD,
    EXB_HTTP_M_GET,
    EXB_HTTP_M_POST,
    EXB_HTTP_M_PUT,
    EXB_HTTP_M_PATCH, /*nonstandard?*/
    EXB_HTTP_M_DELETE,
    EXB_HTTP_M_TRACE,
    EXB_HTTP_M_OPTIONS,
    EXB_HTTP_M_OTHER,
};
enum exb_http_input_state {
    EXB_HTTP_I_ST_INIT,

    EXB_HTTP_I_ST_WAITING_FOR_HEADERS,
    EXB_HTTP_I_ST_WAITING_FOR_BODY,

    EXB_HTTP_I_ST_DONE,
    EXB_HTTP_I_ST_DEAD,
};
enum exb_http_parse_state {
    EXB_HTTP_P_ST_INIT,
    EXB_HTTP_P_ST_IN_CHUNKED_BODY, //Already Parsed headers
    EXB_HTTP_P_ST_DONE,
};

enum exb_http_request_body_handling {
    EXB_HTTP_B_DISCARD,
    EXB_HTTP_B_BUFFER,
};
enum exb_request_handler_reason {
    EXB_HTTP_HANDLER_HEADERS,
    EXB_HTTP_HANDLER_BODY,
};

struct exb_http_header {
    struct exb_str_slice key;
    struct exb_str_slice value;
};
struct exb_http_header_map {
    struct exb_http_header headers[EXB_HTTP_HEADER_MAX];
    int len;

    //Indices of headers that are relevant to the HTTP protocol, -1 means not present
    int h_connection_idx; 
    int h_content_length_idx;
    int h_content_type_idx;
    int h_transfer_encoding_idx;
};

struct exb_request_state {
    struct exb_server *server; //not owned, must outlive
    struct exb_eloop *eloop;  //not owned, must outlive
    int socket_fd;
    int input_buffer_len;
    int input_buffer_cap;
    char *input_buffer;

    bool is_chunked;
    bool is_persistent; //Not persistent when: 
                        //HTTP1.0 (with no keepalive) OR HTTP1.1 and "Connection: close"
    bool is_read_scheduled; /*The request is scheduled to read to*/
    bool is_send_scheduled; /*The request is scheduled to send to*/
    
    bool is_forked; //just a sanity check
    bool is_cancelled; //this can be done better
    
    unsigned char http_major;
    unsigned char http_minor;
    
    int content_length; //only there in messages with a body AND (!is_chunked)
    int bytes_read; //doesn't care about encoding
    int parse_chunk_cursor; //only in chunked requests
    int next_request_cursor;
    
    enum exb_http_input_state istate; //what portion did we receive yet
    enum exb_http_parse_state pstate; //what portion did we parse
    enum exb_http_request_body_handling body_handling;
    enum EXB_HTTP_METHOD method;
    struct exb_http_header_map headers;
    
    struct exb_str_slice method_s;
    struct exb_str_slice path_s;
    struct exb_str_slice version_s;
    struct exb_str_slice status_s; //excluding its crlf
    struct exb_str_slice headers_s; //excluding status's crlf and excluding final crlfcrlf
    struct exb_str_slice body_s; //beginning after crlfcrlf
    
    struct exb_str body_decoded; //TODO: temporary, get rid of this, currently we copy the body to this no matteer what encoding
    struct exb_response_state resp;

    //only relevant when used as a linked list
    struct exb_request_state * next_rqstate;
};

//boolean
static int exb_request_http_version_eq(struct exb_request_state *rqstate, int major, int minor) {
    return rqstate->http_major == major && rqstate->http_minor == minor;
}

void exb_request_repr(struct exb_request_state *rqstate);

static int exb_request_input_buffer_size(struct exb_request_state *rqstate) {
    return rqstate->input_buffer_cap;
}

static int exb_request_input_buffer_ensure_cap(struct exb_request_state *rqstate, size_t capacity) {
    if (capacity > rqstate->input_buffer_cap) {
        struct exb_error err = exb_eloop_realloc_buffer(rqstate->eloop, rqstate->input_buffer, capacity, &rqstate->input_buffer, &rqstate->input_buffer_cap);
        return err.error_code;
    }
    return EXB_OK;
}

static int exb_request_state_init(struct exb_request_state *rqstate, struct exb_eloop *eloop, struct exb *exb, struct exb_server *s, int socket_fd) {
    
    rqstate->is_chunked = 0;
    rqstate->eloop = eloop;
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
    rqstate->headers.len = 0;
    rqstate->next_rqstate = NULL;
    struct exb_error err = exb_eloop_alloc_buffer(eloop, HTTP_INPUT_BUFFER_INITIAL_SIZE, &rqstate->input_buffer, &rqstate->input_buffer_cap);
    if (err.error_code) {
        return err.error_code;
    }

    exb_str_init_empty(&rqstate->body_decoded);
    return exb_response_state_init(&rqstate->resp, rqstate, eloop);
}


static int exb_request_body_bytes_read(struct exb_request_state *rqstate) {
    return rqstate->bytes_read - rqstate->body_s.index;
}

static void exb_request_state_deinit(struct exb_request_state *rqstate, struct exb *exb) {
    exb_response_state_deinit(&rqstate->resp, exb, rqstate->eloop);
    exb_eloop_release_buffer(rqstate->eloop, rqstate->input_buffer, rqstate->input_buffer_cap);
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