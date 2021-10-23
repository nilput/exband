#include "http_server_listener.h"
struct exb_server;
int exb_server_listener_select_new(struct exb_server *s, struct exb_evloop *evloop, struct exb_server_listener **listener);
