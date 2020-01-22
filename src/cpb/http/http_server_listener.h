#ifndef CPB_HTTP_SERVER_LISTENER_H
#define CPB_HTTP_SERVER_LISTENER_H
#include "../cpb.h"

struct cpb_server;
struct cpb_server_listener_fdlist;
//select or epoll or whatever
struct cpb_server_listener {
    int (*listen)(struct cpb_server *s,  struct cpb_server_listener *lis);
    int (*close_connection)(struct cpb_server *s, struct cpb_server_listener *lis, int socket_fd);
    int (*new_connection)(struct cpb_server *s, struct cpb_server_listener *lis, int socket_fd);
    int (*destroy)(struct cpb_server *s, struct cpb_server_listener *lis);
    int (*get_fds)(struct cpb_server *s, struct cpb_server_listener *lis, struct cpb_server_listener_fdlist **fdlist_out);
};

struct cpb_server_listener_fdlist {
    int *fds;
    int len;
};
static void cpb_server_listener_fdlist_destroy(struct cpb *cpb, struct cpb_server_listener_fdlist *fdlist) {
    if (fdlist)
        cpb_free(cpb, fdlist->fds);
    cpb_free(cpb, fdlist);
}



#endif // CPB_HTTP_SERVER_LISTENER_H