#include "eloop.h"
#include <stdio.h>
#include <unistd.h>

static void handle_http(struct cpb_event ev);
static void destroy_http(struct cpb_event ev);

struct cpb_event_handler_itable cpb_event_handler_http_itable = {
    .handle = handle_http,
    .destroy = destroy_http,
};

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

static void handle_http(struct cpb_event ev) {
}
static void destroy_http(struct cpb_event ev) {
}
