#include "http_server.h"
static struct cpb_http_multiplexer *cpb_server_get_multiplexer_i(struct cpb_server *s, int socket_fd) 
{
    if (socket_fd > CPB_SOCKET_MAX)
        return NULL;
    cpb_assert_h(socket_fd >= 0, "invalid socket no");
    return s->mp + socket_fd;
}
static void cpb_server_on_read_available_i(struct cpb_server *s, struct cpb_http_multiplexer *m) {
    struct cpb_event ev;
    cpb_assert_h((!!m) && m->state == CPB_MP_ACTIVE, "");
    cpb_assert_h(!!m->creading, "");
    cpb_assert_h(!m->creading->is_read_scheduled, "");
    
    m->creading->is_read_scheduled = 1;
    cpb_event_http_init(&ev, CPB_HTTP_READ, m->creading, 0);
    cpb_eloop_append(m->eloop, ev);

    RQSTATE_EVENT(stderr, "Scheduled rqstate %p to be read, because we found out "
                    "read is available for socket %d\n", m->creading, m->socket_fd);
    
}
static void cpb_server_on_write_available_i(struct cpb_server *s, struct cpb_http_multiplexer *m) {
    struct cpb_event ev;
    cpb_event_http_init(&ev, CPB_HTTP_SEND, m->next_response, 0);
    cpb_eloop_append(m->eloop, ev);
    m->next_response->is_send_scheduled = 1;
}