#include <stdio.h>
#include "server.h"
int main(int argc, char *argv[]) {
    struct cpb_server server;
    cpb_server_init(&server, 8081);
    cpb_server_listen(&server);
}
