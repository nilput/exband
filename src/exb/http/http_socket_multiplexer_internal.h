#ifndef EXB_HTTP_SOCKET_MULTIPLEXER_INTERNAL_H
#define EXB_HTTP_SOCKET_MULTIPLEXER_INTERNAL_H

#include "../exb.h"
#include "http_socket_multiplexer.h"
#ifdef EXB_WITH_SSL
#include "http_socket_multiplexer_ssl.h"
#endif
#include <string.h>

static void on_http_send_sync(struct exb_event ev);
static void on_http_read_sync(struct exb_event ev);
static void on_http_send_async(struct exb_event ev);
static void on_http_read_async(struct exb_event ev);

static void on_http_ssl_send_sync(struct exb_event ev);
static void on_http_ssl_read_sync(struct exb_event ev);
static void on_http_ssl_send_async(struct exb_event ev);
static void on_http_ssl_read_async(struct exb_event ev);


static void exb_http_multiplexer_init_clear(struct exb_http_multiplexer *mp, int count) {
    memset(mp, 0, sizeof(mp[0]) * count);
    for (int i=0; i<count; i++) {
        mp->state = EXB_MP_EMPTY;
    }
}

static int exb_http_multiplexer_init(struct exb_http_multiplexer *mp,
                                     struct exb_server *s,
                                     struct exb_evloop *evloop,
                                     int evloop_idx,
                                     int socket_fd,
                                     bool is_ssl,
                                     bool is_async) 
{
    #ifndef EXB_WITH_SSL
        if (is_ssl)
            return EXB_UNSUPPORTED;
    #endif

    mp->state = EXB_MP_EMPTY;
    mp->evloop_idx = evloop_idx;
    mp->evloop = evloop;
    mp->socket_fd = socket_fd;
    mp->currently_reading = NULL;
    mp->next_response = NULL;
    mp->wants_read  = false;
    mp->wants_write = false;

    mp->is_ssl = is_ssl;

    if (EXB_UNLIKELY(is_async)) {
        if (EXB_UNLIKELY(is_ssl)) {
            mp->on_read = on_http_ssl_read_async;
            mp->on_send = on_http_ssl_send_async;
        }
        else {
            mp->on_read = on_http_read_async;
            mp->on_send = on_http_send_async;
        }
    }
    else {
        if (EXB_UNLIKELY(is_ssl)) {
            mp->on_read = on_http_ssl_read_sync;
            mp->on_send = on_http_ssl_send_sync;
        }
        else {
            mp->on_read = on_http_read_sync;
            mp->on_send = on_http_send_sync;
        }
    }
#ifdef EXB_WITH_SSL
    if (is_ssl) {
        return exb_http_multiplexer_ssl_init(s, mp);
    }
#endif
    return EXB_OK;
}

static void exb_http_multiplexer_deinit(struct exb_server *s, struct exb_http_multiplexer *mp) {
#ifdef EXB_WITH_SSL
    if (mp->is_ssl) {
        exb_http_multiplexer_ssl_deinit(s, mp);
    }
#endif
    mp->state = EXB_MP_DEAD;
    mp->evloop = NULL;
    mp->evloop_idx = -1;
    mp->socket_fd = -1;
    mp->currently_reading = NULL;
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