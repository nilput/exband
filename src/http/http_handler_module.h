#ifndef CPB_HTTP_HANDLER_MODULE_H
#define CPB_HTTP_HANDLER_MODULE_H
#include "../cpb.h"
struct cpb_server;
struct cpb_request_state;
struct cpb_http_handler_module {
    int (*handle_request)(struct cpb_http_handler_module *module, struct cpb_request_state *rqstate, int reason);
    void (*destroy)(struct cpb_http_handler_module *module, struct cpb *cpb);
};

/*
when a config has http_handler=foo.so:handler
foo.so is loaded
we search for "handler_init()" in foo.so and call it (formed by concatanating the handler name and "_init")
int handler_init(struct cpb *cpb, struct cpb_server *server, struct cpb_http_handler_module **module_out);
at http requests module_out->handle_request() is called
at reload / destruction module_out->destroy() is called
*/
//returning non-zero value means error
typedef int (*cpb_handler_init_func)(struct cpb *cpb, struct cpb_server *server, char *module_args, struct cpb_http_handler_module **module_out);

//internal
//handler name is not owned

int cpb_http_handler_module_load(struct cpb *cpb_ref, struct cpb_server *server, char *handler_name, char *module_args, struct cpb_http_handler_module **module_out, void **handle_out);
int cpb_http_handler_module_unload(struct cpb *cpb_ref, struct cpb_http_handler_module *module, void *handle);
#endif //CPB_HTTP_HANDLER_MODULE_H