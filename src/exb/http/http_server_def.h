
#ifndef EXB_SERVER_DEF_H
#define EXB_SERVER_DEF_H
#include "../exb_event.h"
#include "../exb_build_config.h"
#include "http_request_handler.h"
#include "http_request_state_recycle_array.h"
#include "http_socket_multiplexer.h"
#include "http_server_config.h"
#include "io_result.h"
struct exb_pcontrol;
struct exb_eloop_pool;

struct exb_http_ssl_state;
struct exb_http_multiplexer;

struct exb_ssl_interface {
    struct exb_http_server_module *module;
    int (*ssl_connection_init)(struct exb_http_server_module *module, struct exb_http_multiplexer *mp);
    struct exb_io_result (*ssl_connection_read)(struct exb_http_server_module *module, struct exb_http_multiplexer *mp, char *buffer, size_t buffer_max);
    struct exb_io_result (*ssl_connection_write)(struct exb_http_server_module *module, struct exb_http_multiplexer *mp, char *buffer, size_t buffer_len);
    void (*ssl_connection_deinit)(struct exb_http_server_module *module, struct exb_http_multiplexer *mp);
};

struct exb_server {
    struct exb *exb;                 //not owned, must outlive
    struct exb_eloop_pool *elist;    //not owned, must outlive
    struct exb_pcontrol *pcontrol;   //not owned, must outlive

    struct {
        int socket_fd;
        int port;
        bool is_ssl;
    } listen_sockets[EXB_MAX_DOMAINS];
    int n_listen_sockets;

    struct {
        struct exb_server_listener *listener;
        struct exb_request_state_recycle_array rq_cyc;
    } loop_data[EXB_MAX_ELOOPS];

    void *request_handler_state; 
    exb_request_handler_func request_handler;

    struct exb_http_server_config config; //owned

    struct {
        struct exb_http_server_module *module;
        void *dll_module_handle;
        int missing_symbols;
    } loaded_modules[EXB_SERVER_MAX_MODULES];
    int n_loaded_modules;

    struct exb_ssl_interface ssl_interface;

    struct exb_http_multiplexer mp[EXB_SOCKET_MAX] EXB_ALIGN(64);

};

#endif
