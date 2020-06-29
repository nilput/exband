#ifndef EXB_HTTP_RESPONSE_DEF_H
#define EXB_HTTP_RESPONSE_DEF_H


#define HTTP_HEADERS_BUFFER_INIT_SIZE 512
#define HTTP_OUTPUT_BUFFER_INIT_SIZE 2048
#define EXB_HTTP_RESPONSE_HEADER_MAX 16
#define HTTP_STATUS_MAX_SZ 64

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

#endif