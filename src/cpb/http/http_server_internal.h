#ifndef CPB_HTTP_SERVER_INTERNAL_H
#define CPB_HTTP_SERVER_INTERNAL_H
#include "http_server.h"
static inline struct cpb_http_multiplexer *cpb_server_get_multiplexer_i(struct cpb_server *s, int socket_fd) 
{
    if (socket_fd > CPB_SOCKET_MAX)
        return NULL;
    cpb_assert_h(socket_fd >= 0, "invalid socket no");
    return s->mp + socket_fd;
}

#endif // CPB_HTTP_SERVER_INTERNAL_H