#ifndef CPB_HTTP_SERVER_MODULE_INT_H
#define CPB_HTTP_SERVER_MODULE_INT_H
//handler name is not owned
int cpb_http_server_module_load(struct cpb *cpb_ref, struct cpb_server *server, char *handler_name, char *module_args, struct cpb_http_server_module **module_out, void **handle_out);
int cpb_http_server_module_unload(struct cpb *cpb_ref, struct cpb_http_server_module *module, void *handle);
#endif // CPB_HTTP_SERVER_MODULE_INT_H