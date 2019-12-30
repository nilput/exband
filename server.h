#include "errors.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define LISTEN_BACKLOG 16

define_cpb_or(int, struct cpb_or_socket);
struct cpb_server {
    int port;
    int listen_socket_fd;
    fd_set active_fd_set;
    fd_set read_fd_set;
    struct sockaddr_in clientname;
};

struct cpb_error cpb_server_init(struct cpb_server *s, int port);
struct cpb_error cpb_server_listen(struct cpb_server *s);
void cpb_server_deinit(struct cpb_server *s);