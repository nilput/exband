#include "http_server.h"
static struct cpb_http_multiplexer *cpb_server_get_multiplexer_i(struct cpb_server *s, int socket_fd) 
{
    if (socket_fd > CPB_SOCKET_MAX)
        return NULL;
    cpb_assert_h(socket_fd >= 0, "invalid socket no");
    return s->mp + socket_fd;
}