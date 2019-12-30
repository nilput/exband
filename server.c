#include <sys/select.h>
#include <unistd.h>
#include "errors.h"
#include "server.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <fcntl.h> /* Added for the nonblocking socket */
//https://www.gnu.org/software/libc/manual/html_node/Server-Example.html
//http://www.cs.tau.ac.il/~eddiea/samples/Non-Blocking/tcp-nonblocking-server.c.html
static struct cpb_or_socket make_socket (uint16_t port)
{
    int sock = socket (PF_INET, SOCK_STREAM, 0);
    struct cpb_or_socket rv = {0};
    if (sock < 0) {
        rv.error = cpb_make_error(CPB_SOCKET_ERR);
        return rv;
    }

    /* Give the socket a name. */
    struct sockaddr_in name;
    name.sin_family = AF_INET;
    name.sin_port = htons(port);
    name.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind (sock, (struct sockaddr *) &name, sizeof (name)) < 0) {
        rv.error = cpb_make_error(CPB_BIND_ERR);
        return rv;
    }
    fcntl(sock, F_SETFL, O_NONBLOCK); /* Change the socket into non-blocking state	*/
    rv.value = sock;
    return rv;
}
struct cpb_error cpb_server_init(struct cpb_server *s, int port) {
    struct cpb_error err = {0};
    /* Create the socket and set it up to accept connections. */
    struct cpb_or_socket or_socket = make_socket(port);
    if (or_socket.error.error_code) {
        return cpb_prop_error(or_socket.error);
    }
    int socket_fd = or_socket.value;
    s->listen_socket_fd = socket_fd;
    return err;
}
struct cpb_error read_from_client(int socket) {
    char buf[512];
    int nbytes = read(socket, buf, 512);
    struct cpb_error err = {0};
    if (nbytes < 0) {
        err = cpb_make_error(CPB_READ_ERR);
    }
    else if (nbytes == 0) {
        err = cpb_make_error(CPB_EOF);
    }
    else {
        fprintf(stderr, "Server: got message: `%s'\n", buf);
    }
    return err;
}
struct cpb_error cpb_server_listen(struct cpb_server *s) {
    struct cpb_error err = {0};
    if (listen(s->listen_socket_fd, LISTEN_BACKLOG) < 0) { 
        err = cpb_make_error(CPB_LISTEN_ERR);
        return err;
    }

    /* Initialize the set of active sockets. */
    FD_ZERO(&s->active_fd_set);
    FD_SET(s->listen_socket_fd, &s->active_fd_set);

    while (1) {
        /* Block until input arrives on one or more active sockets. */
        s->read_fd_set = s->active_fd_set;
        struct timeval timeout = {0, 0}; //poll
        if (select(FD_SETSIZE, &s->read_fd_set, NULL, NULL, &timeout) < 0) {
            err = cpb_make_error(CPB_SELECT_ERR);
            return err;
        }

        /* Service all the sockets with input pending. */
        for (int i = 0; i < FD_SETSIZE; ++i) {
            if (FD_ISSET(i, &s->read_fd_set)) {
                if (i == s->listen_socket_fd) {
                    /* Connection request on original socket. */
                    int new_socket;
                    socklen_t size = sizeof(s->clientname);
                    new_socket = accept(s->listen_socket_fd,
                                (struct sockaddr *) &s->clientname,
                                &size);
                    if (new_socket < 0) {
                        err = cpb_make_error(CPB_ACCEPT_ERR);
                        return err;
                    }
                    fcntl(new_socket, F_SETFL, O_NONBLOCK); /* Change the socket into non-blocking state	*/
                    fprintf(stderr,
                            "Server: connect from host %s, port %hd.\n",
                            inet_ntoa (s->clientname.sin_addr),
                            ntohs (s->clientname.sin_port));
                    FD_SET(new_socket, &s->active_fd_set);
                }
                else {
                    /* Data arriving on an already-connected socket. */
                    struct cpb_error err = read_from_client(i);
                    if (err.error_code != 0) {
                        close(i);
                        FD_CLR(i, &s->active_fd_set);
                    }
                }
            }
        }
    }
}
void cpb_server_deinit(struct cpb_server *s) {

}