#ifndef EXB_HTTP_SERVER_MODULE_H
#define EXB_HTTP_SERVER_MODULE_H
#include "../exb.h"
#include "http_request_handler.h"
struct exb_server;

struct exb_http_server_module {
    void (*destroy)(struct exb_http_server_module *module, struct exb *exb);
};

/*
Server plugin interface


when a config has http_module=foo.so:handler
foo.so is loaded
we search for "handler_init()" in foo.so and call it (formed by concatanating the handler name and "_init")
int handler_init(struct exb *exb, struct exb_server *server, struct exb_http_server_module **module_out);
if a module registers itself as a request handler
    at http requests its passed request_handler func is called
at reload / destruction module_out->destroy() is called
*/
//returning non-zero value means error
typedef int (*exb_http_server_module_init_func)(struct exb *exb, struct exb_server *server, char *module_args, struct exb_http_server_module **module_out);

int  exb_server_set_request_handler(struct exb_server *s, void *handler_state, exb_request_handler_func func);
#endif //EXB_HTTP_SERVER_MODULE_H