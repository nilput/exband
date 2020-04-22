#ifndef HTTP_SOCKET_MULTIPLEXER_H
#define HTTP_SOCKET_MULTIPLEXER_H
#include <netinet/in.h>
enum exb_http_multiplexer_state {
    EXB_MP_EMPTY,
    EXB_MP_ACTIVE,
    EXB_MP_CANCELLING,
    EXB_MP_DEAD, //should never be seen
};
struct exb_http_multiplexer {
    enum exb_http_multiplexer_state state EXB_ALIGN(64); 
    int eloop_idx;
    int socket_fd;
    bool wants_read;  //caching whether creading exists / is scheduled or not
    bool wants_write; //caching whether next_response is ready or not
    struct exb_eloop *eloop;
    struct exb_request_state *creading; //the current request reading from client
    struct exb_request_state *next_response; //queue of responses (linkedlist)
    struct sockaddr_in clientname;
};
static void exb_http_multiplexer_init(struct exb_http_multiplexer *mp, struct exb_eloop *eloop, int eloop_idx, int socket_fd) {
    mp->state = EXB_MP_EMPTY;
    mp->eloop_idx = eloop_idx;
    mp->eloop = eloop;
    mp->socket_fd = socket_fd;
    mp->creading = NULL;
    mp->next_response = NULL;
    mp->wants_read  = 0;
    mp->wants_write = 0;
}

static void exb_http_multiplexer_deinit(struct exb_http_multiplexer *mp) {
    mp->state = EXB_MP_DEAD;
    mp->eloop = NULL;
    mp->eloop_idx = -1;
    mp->socket_fd = -1;
    mp->creading = NULL;
    mp->next_response = NULL;
    mp->wants_read  = 0;
    mp->wants_write = 0;
}
static void exb_http_multiplexer_queue_response(struct exb_http_multiplexer *mp, struct exb_request_state *rqstate) {
    
    if (mp->next_response == NULL) {
        mp->next_response = rqstate;
        mp->wants_write = mp->next_response && mp->next_response->resp.state == EXB_HTTP_R_ST_SENDING;
    }
    else {
        struct exb_request_state *tail = mp->next_response;
        while (tail->next_rqstate) {
            exb_assert_s(tail != tail->next_rqstate, "");
            tail = tail->next_rqstate;
        }
        exb_assert_s(tail != rqstate, "");
        tail->next_rqstate = rqstate;
    }
}
static void exb_http_multiplexer_pop_response(struct exb_http_multiplexer *mp) {
    if (mp->next_response) {
        struct exb_request_state *next = mp->next_response->next_rqstate;
        mp->next_response = next;
    }
    mp->wants_write = mp->next_response && mp->next_response->resp.state == EXB_HTTP_R_ST_SENDING;
}
#endif