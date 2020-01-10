#ifndef HTTP_SOCKET_MULTIPLEXER_H
#define HTTP_SOCKET_MULTIPLEXER_H
#include <netinet/in.h>
enum cpb_http_multiplexer_state {
    CPB_MP_EMPTY,
    CPB_MP_ACTIVE,
};
struct cpb_http_multiplexer {
    char state;
    int socket_fd;
    struct sockaddr_in clientname;
    struct cpb_request_state *creading; //the current request reading from client
    struct cpb_request_state *next_response; //queue of responses (linkedlist)
};
static void cpb_http_multiplexer_init(struct cpb_http_multiplexer *mp) {
    mp->state = CPB_MP_EMPTY;
    mp->socket_fd = -1;
    mp->creading = NULL;
    mp->next_response = NULL;
}
static void cpb_http_multiplexer_deinit(struct cpb_http_multiplexer *mp) {
    mp->state = CPB_MP_EMPTY;
    mp->socket_fd = -1;
    mp->creading = NULL;
    mp->next_response = NULL;
}
static void cpb_http_multiplexer_queue_response(struct cpb_http_multiplexer *mp, struct cpb_request_state *rqstate) {
    if (mp->next_response == NULL) {
        mp->next_response = rqstate;
    }
    else {
        struct cpb_request_state *tail = mp->next_response;
        while (tail && tail->next_rqstate)
            tail = tail->next_rqstate;
        tail->next_rqstate = rqstate;
    }
}
static void cpb_http_multiplexer_pop_response(struct cpb_http_multiplexer *mp) {
    if (mp->next_response) {
        struct cpb_request_state *next = mp->next_response->next_rqstate;
        mp->next_response = next;
    }
}
#endif