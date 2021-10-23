#ifndef EXB_HTTP_SOCKET_MULTIPLEXER_SSL_DEF_H
#define EXB_HTTP_SOCKET_MULTIPLEXER_SSL_DEF_H

struct exb_http_ssl_state {
    //These are specific to openssl,
    // if another SSL implementation is to be used in the future, then a union can be used, 
    // the goal here is to minimize indirections
    void *ssl_obj;
    void *rbio;
    void *wbio;

    //a buffer that is used when we get bytes out of wbio and we have no where to store them
    //this buffer should be recycled to the current evloop, also it can be NULL
    char *keep_buff; 
    int keep_buff_len;
    int keep_buff_size;

    int ssl_context_id;
    int flags;
};

#endif