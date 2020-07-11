#ifndef EXB_SSL_H
#define EXB_SSL_H

struct exb;
struct exb_server;
struct exb_http_server_module;

int exb_ssl_init(struct exb *exb,
                 struct exb_server *server,
                 char *module_args,
                 struct exb_http_server_module **module_out);

#endif