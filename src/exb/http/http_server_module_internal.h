#ifndef EXB_HTTP_SERVER_MODULE_INT_H
#define EXB_HTTP_SERVER_MODULE_INT_H
//handler name is not owned
int exb_http_server_module_load(struct exb *exb_ref, struct exb_server *server, char *handler_name, char *module_args, struct exb_http_server_module **module_out, void **handle_out);
int exb_http_server_module_unload(struct exb *exb_ref, struct exb_http_server_module *module, void *handle);
#endif // EXB_HTTP_SERVER_MODULE_INT_H