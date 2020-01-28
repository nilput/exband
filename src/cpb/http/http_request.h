#ifndef CPB_HTTP_REQUEST_H
#define CPB_HTTP_REQUEST_H
#include "../cpb_str.h"
#include "http_response.h"
#include <stdbool.h>



#define CPB_HTTP_HEADER_MAX 32
#define HTTP_INPUT_BUFFER_INITIAL_SIZE 2048




enum CPB_HTTP_METHOD {
    CPB_HTTP_M_HEAD,
    CPB_HTTP_M_GET,
    CPB_HTTP_M_POST,
    CPB_HTTP_M_PUT,
    CPB_HTTP_M_PATCH, /*nonstandard?*/
    CPB_HTTP_M_DELETE,
    CPB_HTTP_M_TRACE,
    CPB_HTTP_M_OPTIONS,
    CPB_HTTP_M_OTHER,
};
enum cpb_http_input_state {
    CPB_HTTP_I_ST_INIT,

    CPB_HTTP_I_ST_WAITING_FOR_HEADERS,
    CPB_HTTP_I_ST_WAITING_FOR_BODY,

    CPB_HTTP_I_ST_DONE,
    CPB_HTTP_I_ST_DEAD,
};
enum cpb_http_parse_state {
    CPB_HTTP_P_ST_INIT,
    CPB_HTTP_P_ST_IN_CHUNKED_BODY, //Already Parsed headers
    CPB_HTTP_P_ST_DONE,
};

enum cpb_http_request_body_handling {
    CPB_HTTP_B_DISCARD,
    CPB_HTTP_B_BUFFER,
};
enum cpb_request_handler_reason {
    CPB_HTTP_HANDLER_HEADERS,
    CPB_HTTP_HANDLER_BODY,
};

struct cpb_http_header {
    struct cpb_str_slice key;
    struct cpb_str_slice value;
};
struct cpb_http_header_map {
    struct cpb_http_header headers[CPB_HTTP_HEADER_MAX];
    int len;

    //Indices of headers that are relevant to the HTTP protocol, -1 means not present
    int h_connection_idx; 
    int h_content_length_idx;
    int h_content_type_idx;
    int h_transfer_encoding_idx;
};

struct cpb_request_state {
    struct cpb_server *server; //not owned, must outlive
    struct cpb_eloop *eloop;  //not owned, must outlive

    unsigned char http_major;
    unsigned char http_minor;


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
    
    int content_length; //only there in messages with a body AND (!is_chunked)
    int bytes_read; //doesn't care about encoding
    int parse_chunk_cursor; //only in chunked requests
    int next_request_cursor;
    
    enum cpb_http_input_state istate; //what portion did we recieve yet
    enum cpb_http_parse_state pstate; //what portion did we parse
    enum cpb_http_request_body_handling body_handling;
    enum CPB_HTTP_METHOD method;
    struct cpb_http_header_map headers;
    
    struct cpb_str_slice method_s;
    struct cpb_str_slice path_s;
    struct cpb_str_slice version_s;
    struct cpb_str_slice status_s; //excluding its crlf
    struct cpb_str_slice headers_s; //excluding status's crlf and excluding final crlfcrlf
    struct cpb_str_slice body_s; //beginning after crlfcrlf
    
    struct cpb_str body_decoded; //TODO: temporary, get rid of this, currently we copy the body to this no matteer what encoding

    
    struct cpb_response_state resp;

    //only relevant when used as a linked list
    struct cpb_request_state * next_rqstate;

    
};

//boolean
static int cpb_request_http_version_eq(struct cpb_request_state *rqstate, int major, int minor) {
    return rqstate->http_major == major && rqstate->http_minor == minor;
}

void cpb_request_repr(struct cpb_request_state *rqstate);

static int cpb_request_input_buffer_size(struct cpb_request_state *rqstate) {
    return rqstate->input_buffer_cap;
}

static int cpb_request_input_buffer_ensure_cap(struct cpb_request_state *rqstate, size_t capacity) {
    if (capacity > rqstate->input_buffer_cap) {
        struct cpb_error err = cpb_eloop_realloc_buffer(rqstate->eloop, rqstate->input_buffer, capacity, &rqstate->input_buffer, &rqstate->input_buffer_cap);
        return err.error_code;
    }
    return CPB_OK;
}

static int cpb_request_state_init(struct cpb_request_state *rqstate, struct cpb_eloop *eloop, struct cpb *cpb, struct cpb_server *s, int socket_fd) {
    
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
    rqstate->istate = CPB_HTTP_I_ST_INIT;
    rqstate->pstate = CPB_HTTP_P_ST_INIT;
    rqstate->body_handling = CPB_HTTP_B_DISCARD;
    rqstate->headers.h_connection_idx        = -1;
    rqstate->headers.h_content_length_idx    = -1;
    rqstate->headers.h_content_type_idx      = -1;
    rqstate->headers.h_transfer_encoding_idx = -1;
    rqstate->headers.len = 0;
    rqstate->next_rqstate = NULL;
    struct cpb_error err = cpb_eloop_alloc_buffer(eloop, HTTP_INPUT_BUFFER_INITIAL_SIZE, &rqstate->input_buffer, &rqstate->input_buffer_cap);
    if (err.error_code) {
        return err.error_code;
    }

    cpb_str_init_empty(&rqstate->body_decoded);
    return cpb_response_state_init(&rqstate->resp, rqstate, eloop);
}


static int cpb_request_body_bytes_read(struct cpb_request_state *rqstate) {
    return rqstate->bytes_read - rqstate->body_s.index;
}

static void cpb_request_state_deinit(struct cpb_request_state *rqstate, struct cpb *cpb) {
    cpb_response_state_deinit(&rqstate->resp, cpb, rqstate->eloop);
    cpb_eloop_release_buffer(rqstate->eloop, rqstate->input_buffer, rqstate->input_buffer_cap);
    cpb_str_deinit(cpb, &rqstate->body_decoded);
    rqstate->istate = CPB_HTTP_I_ST_DEAD;
}

static int cpb_request_has_body(struct cpb_request_state *rqstate) {
    return (rqstate->headers.h_content_length_idx != -1) || (rqstate->headers.h_transfer_encoding_idx != -1);
}
static int cpb_request_is_chunked_body_complete(struct cpb_request_state *rqstate) {
    return rqstate->pstate == CPB_HTTP_P_ST_DONE;
}

static int cpb_request_http_check_validity(struct cpb_request_state *rqstate) {
    int has_body = cpb_request_has_body(rqstate);
    /*Conditions we are checking for:*/
    /*  Body must not be present in Trace*/
    /*  in Requests with bodies, Content-Length or Chunked encoding must be used, otherwise we can't respond*/
    if (has_body && rqstate->method == CPB_HTTP_M_TRACE)
        return CPB_HTTP_ERROR;
    return CPB_OK;
}


#endif// CPB_HTTP_REQUEST_H