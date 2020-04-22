#ifndef EXB_HTTP_SERVER_LISTENER_H
#define EXB_HTTP_SERVER_LISTENER_H
#include "../exb.h"

struct exb_server;
struct exb_eloop;
struct exb_server_listener_fdlist;
//select or epoll or whatever
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