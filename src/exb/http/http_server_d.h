
#ifndef EXB_SERVER_DEF_H
#define EXB_SERVER_DEF_H
#include "../exb_event.h"
#include "../exb_build_config.h"
#include "http_request_handler.h"
#include "http_request_state_recycle_array.h"
#include "http_socket_multiplexer.h"
#include "http_server_config.h"
struct exb_pcontrol;
struct exb_eloop_pool;



struct exb_server {
    struct exb *exb;                 //not owned, must outlive
    struct exb_eloop_pool *elist;    //not owned, must outlive
    struct exb_pcontrol *pcontrol;   //not owned, must outlive

    void (*on_read)(struct exb_event ev);
    void (*on_send)(struct exb_event ev);

    int port;
    int listen_socket_fd;

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
    } loaded_modules[EXB_SERVER_MAX_MODULES];
    int n_loaded_modules;

    struct exb_http_multiplexer mp[EXB_SOCKET_MAX] EXB_ALIGN(64);
};

#endif