#ifndef EXB_HTTP_SOCKET_MULTIPLEXER_SSL_H
#define EXB_HTTP_SOCKET_MULTIPLEXER_SSL_H
#include "http_socket_multiplexer_ssl_def.h"
#include "../exb_errors.h"

struct exb_http_multiplexer;

static int exb_http_multiplexer_ssl_init(struct exb_server *s, struct exb_http_multiplexer *mp)
{
    return s->ssl_interface.ssl_connection_init(s->ssl_interface.module, mp);
}

static void exb_http_multiplexer_ssl_deinit(struct exb_server *s, struct exb_http_multiplexer *mp)
{
    s->ssl_interface.ssl_connection_deinit(s->ssl_interface.module, mp);
}

#endif