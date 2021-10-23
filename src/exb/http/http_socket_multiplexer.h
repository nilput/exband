#ifndef HTTP_SOCKET_MULTIPLEXER_H
#define HTTP_SOCKET_MULTIPLEXER_H
#include <netinet/in.h>
#include "http_request_def.h"
#include "../exb_event.h"
#ifdef EXB_WITH_SSL
    #include "http_socket_multiplexer_ssl_def.h"
#endif
enum exb_http_multiplexer_state {
    EXB_MP_EMPTY,
    EXB_MP_ACTIVE,
    EXB_MP_CANCELLING,
    EXB_MP_DEAD, //should never be seen
};
struct exb_http_multiplexer {
    enum exb_http_multiplexer_state state EXB_ALIGN(64); 
    int evloop_idx;
    int socket_fd;
    struct exb_evloop *evloop; //TODO: refactor, either store evloop_idx or evloop
    
    struct exb_request_state *currently_reading; //the current request reading from client
    struct exb_request_state *next_response; //queue of responses (linkedlist)
    void (*on_read)(struct exb_event ev);
    void (*on_send)(struct exb_event ev);

    struct sockaddr_in clientname;
    bool wants_read;  //caching whether currently_reading exists / is scheduled or not
    bool wants_write; //caching whether next_response is ready or not
    bool is_ssl;
#ifdef EXB_WITH_SSL
    struct exb_http_ssl_state ssl_state;
#endif
};

#endif
