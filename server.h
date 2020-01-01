#include <sys/types.h>
#include <sys/socket.h>

#include "errors.h"
#include "http_request.h"


#define LISTEN_BACKLOG 16
#define CPB_SOCKET_MAX 8
#define CPB_HTTP_MIN_DELAY 10 //ms



define_cpb_or(int, struct cpb_or_socket);

/*
    In the beginning there was Server 
    Then Server said let there be Light

    Server -> (needs an eloop)
        has a bunch of roles:  Accept (see whether there are new connections, if so accept them)
                               Select (see whether there are sockets available for read/write, if
                                       Schedule the appropriate handler in the event loop)

        Server schedules itself to be ran in the event loop too
        it manages the lifetime of requests and stores their state, event refer to the request's state

*/


struct cpb_server {
    struct cpb *cpb; //not owned, must outlive
    struct cpb_eloop *eloop; //not owned, must outlive
    int port;
    int listen_socket_fd;
    fd_set active_fd_set;
    fd_set read_fd_set;    
    
    int nrequests;
    struct cpb_request_state requests[CPB_SOCKET_MAX];
};

struct cpb_error cpb_server_init(struct cpb_server *s, struct cpb *cpb_ref, struct cpb_eloop *eloop, int port);
struct cpb_error cpb_server_listen(struct cpb_server *s);
void cpb_server_deinit(struct cpb_server *s);
