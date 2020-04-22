#include "http_server_listener.h"
struct cpb_server;
int cpb_server_listener_select_new(struct cpb_server *s, struct cpb_eloop *eloop, struct cpb_server_listener **listener);
