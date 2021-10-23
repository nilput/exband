#ifndef EXB_HTTP_SERVER_LISTENER_H
#define EXB_HTTP_SERVER_LISTENER_H
#include "../exb.h"

struct exb_server;
struct exb_evloop;
struct exb_server_listener_fdlist;
/* an interface for a concrete implementation, be for example: select or epoll */
/*
    This has the following responsibilities:
        * when connections are available on the main listening socket
          calls exb_server_accept_new_connections()
        * when read is available on a tracked socket and the socket 
          is marked to accept reading
          calls exb_server_on_read_available
        * when write is available on a tracked socket and it accepts writing
          calls exb_server_on_write_available
        * implements exb_server_listener_epoll_get_fds which returns an
          array of integers for all tracked sockets, this is used during switching
          implementation to another one
        * implements listen() which doesn't block for a long time, but only 
          checks if current socket events are there and handles them as mentioned
        * implements destroy() which cleans up allocated data
        * implements close_connection(socket_fd) which is called before the server
          closes the socket
        * implements new_connection(socket_fd) which starts tracking the socket passed to it

*/
struct exb_server_listener {
    int (*listen)(struct exb_server_listener *lis);
    int (*close_connection)(struct exb_server_listener *lis, int socket_fd);
    int (*new_connection)(struct exb_server_listener *lis, int socket_fd);
    int (*destroy)(struct exb_server_listener *lis);
    int (*get_fds)(struct exb_server_listener *lis, struct exb_server_listener_fdlist **fdlist_out);
};

struct exb_server_listener_fdlist {
    int *fds;
    int len;
};
static void exb_server_listener_fdlist_destroy(struct exb *exb, struct exb_server_listener_fdlist *fdlist) {
    if (fdlist)
        exb_free(exb, fdlist->fds);
    exb_free(exb, fdlist);
}
#endif // EXB_HTTP_SERVER_LISTENER_H