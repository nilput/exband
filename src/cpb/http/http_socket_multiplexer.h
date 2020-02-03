#ifndef HTTP_SOCKET_MULTIPLEXER_H
#define HTTP_SOCKET_MULTIPLEXER_H
#include <netinet/in.h>
enum cpb_http_multiplexer_state {
    CPB_MP_EMPTY,
    CPB_MP_ACTIVE,
    CPB_MP_CANCELLING,
    CPB_MP_DEAD, //should never be seen
};
struct cpb_http_multiplexer {
    int eloop_idx;
    struct cpb_eloop *eloop;
    enum cpb_http_multiplexer_state state;
    int socket_fd;
    struct sockaddr_in clientname;
    struct cpb_request_state *creading; //the current request reading from client
    struct cpb_request_state *next_response; //queue of responses (linkedlist)
};
static void cpb_http_multiplexer_init(struct cpb_http_multiplexer *mp, struct cpb_eloop *eloop, int eloop_idx, int socket_fd) {
    mp->state = CPB_MP_EMPTY;
    mp->eloop_idx = eloop_idx;
    mp->eloop = eloop;
    mp->socket_fd = socket_fd;
    mp->creading = NULL;
    mp->next_response = NULL;
}

static void cpb_http_multiplexer_deinit(struct cpb_http_multiplexer *mp) {
    mp->state = CPB_MP_DEAD;
    mp->eloop = NULL;
    mp->eloop_idx = -1;
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
        while (tail->next_rqstate) {
            cpb_assert_s(tail != tail->next_rqstate, "");
            tail = tail->next_rqstate;
        }
        cpb_assert_s(tail != rqstate, "");
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