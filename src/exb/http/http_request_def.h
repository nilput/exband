#ifndef EXB_HTTP_REQUEST_DEF_H
#define EXB_HTTP_REQUEST_DEF_H

#include <stdbool.h>
#include "../exb_msg.h"
#include "http_request_handler.h"
#include "http_response_def.h"

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
    int h_host_idx; 
};

struct exb_request_state {
    exb_request_handler_func request_handler;
    void *rqh_state; //Userdata associated with the request handler, this is usually a pointer to the module
    struct exb_msg userdata;

    struct exb_server *server; //not owned, must outlive
    struct exb_evloop *evloop;  //not owned, must outlive
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

#endif
