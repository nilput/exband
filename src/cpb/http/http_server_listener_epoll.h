#include "http_server_listener.h"
struct cpb_server;
int cpb_server_listener_epoll_new(struct cpb_server *s, struct cpb_server_listener **listener);
int cpb_server_listener_switch_to_epoll(struct cpb_server *s);