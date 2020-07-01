#ifndef EXB_HTTP_SERVER_MODULE_INT_H
#define EXB_HTTP_SERVER_MODULE_INT_H
#include "http_request_handler.h"

//handler name, args, and import list are not owned
struct exb_str_list;
int exb_http_server_module_load(struct exb *exb_ref,
                                struct exb_server *server,
                                int module_id,
                                char *handler_name,
                                char *module_args,
                                struct exb_str_list *import_list,
                                struct exb_http_server_module **module_out,
                                void **handle_out);

int exb_http_server_module_unload(struct exb *exb_ref, struct exb_http_server_module *module, void *handle);

//Called by module loader
void exb_http_server_module_on_load_resolve_handler(struct exb *exb_ref,
                                                   struct exb_server *server,
                                                   int module_id,
                                                   struct exb_http_server_module *module,
                                                   char *handler_name, 
                                                   exb_request_handler_func func);
                                                   

#endif // EXB_HTTP_SERVER_MODULE_INT_H
