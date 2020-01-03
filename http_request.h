#ifndef CPB_HTTP_REQUEST_H
#define CPB_HTTP_REQUEST_H
#include "str.h"
#include "http_response.h"
#include <stdbool.h>
#include <netinet/in.h>


#define CPB_HTTP_HEADER_MAX 32
#define HTTP_INPUT_BUFFER_SIZE 8192


enum CPB_HTTP_METHOD {
    CPB_HTTP_M_HEAD,
    CPB_HTTP_M_GET,
    CPB_HTTP_M_POST,
    CPB_HTTP_M_PUT,
    CPB_HTTP_M_DELETE,
    CPB_HTTP_M_OPTIONS,
    CPB_HTTP_M_OTHER,
};
enum cpb_http_input_state {
    CPB_HTTP_I_ST_INIT,
    CPB_HTTP_I_ST_GOT_HEADERS,
    CPB_HTTP_I_ST_GOT_BODY,
};
enum cpb_http_parse_state {
    CPB_HTTP_P_ST_INIT,
    CPB_HTTP_P_ST_PARSED_HEADERS,
    CPB_HTTP_P_ST_PARSED_BODY,
};


struct cpb_http_header {
    struct cpb_str_slice key;
    struct cpb_str_slice value;
};
struct cpb_http_header_map {
    struct cpb_http_header headers[CPB_HTTP_HEADER_MAX];
    int len;
};

struct cpb_request_state {
    struct cpb_server *server; //not owned, must outlive
    int socket_fd;
    struct sockaddr_in clientname;
    int input_buffer_len;
    bool is_chunked;
    bool is_persistent; //Not persistent when: 
                        //HTTP1.0 (with no keepalive) OR HTTP1.1 and "Connection: close"
    
    enum cpb_http_input_state istate; //what portion did we recieve yet
    enum cpb_http_parse_state pstate; //what portion did we parse
    struct cpb_http_header_map headers;
    enum CPB_HTTP_METHOD method;
    struct cpb_str_slice method_s;
    struct cpb_str_slice path_s;
    struct cpb_str_slice version_s;
    struct cpb_str_slice status_s; //excluding its crlf
    struct cpb_str_slice headers_s; //excluding status's crlf and excluding final crlfcrlf
    struct cpb_str_slice body_s; //beginning after crlfcrlf

    char input_buffer[HTTP_INPUT_BUFFER_SIZE];
    struct cpb_response_state resp;
};

void cpb_request_repr(struct cpb_request_state *rqstate);

static void cpb_request_state_init(struct cpb_request_state *rqstate, struct cpb_server *s, int socket_fd, struct sockaddr_in clientname) {
    rqstate->clientname = clientname;
    rqstate->socket_fd = socket_fd;
    rqstate->server = s;
    rqstate->input_buffer_len = 0;
    rqstate->istate = CPB_HTTP_I_ST_INIT;
    rqstate->pstate = CPB_HTTP_P_ST_INIT;
    cpb_response_state_init(&rqstate->resp, rqstate);
}


#endif// CPB_HTTP_REQUEST_H