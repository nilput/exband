#ifndef CPB_HTTP_SERVER_MODULE_H
#define CPB_HTTP_SERVER_MODULE_H
#include "../cpb.h"
struct cpb_server;

struct cpb_http_server_module {
    void (*destroy)(struct cpb_http_server_module *module, struct cpb *cpb);
};

/*
when a config has http_module=foo.so:handler
foo.so is loaded
we search for "handler_init()" in foo.so and call it (formed by concatanating the handler name and "_init")
int handler_init(struct cpb *cpb, struct cpb_server *server, struct cpb_http_server_module **module_out);
if a module registers itself as a request handler
    at http requests its passed request_handler func is called
at reload / destruction module_out->destroy() is called
*/
//returning non-zero value means error
typedef int (*cpb_http_server_module_init_func)(struct cpb *cpb, struct cpb_server *server, char *module_args, struct cpb_http_server_module **module_out);

struct cpb_request_state;
typedef int (*cpb_module_request_handler_func)(struct cpb_http_server_module *module, struct cpb_request_state *rqstate, int reason);
int  cpb_server_set_module_request_handler(struct cpb_server *s, struct cpb_http_server_module *mod, cpb_module_request_handler_func func);


#endif //CPB_HTTP_SERVER_MODULE_H