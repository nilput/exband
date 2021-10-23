#ifndef EXB_HTTP_REQUEST_HANDLER_H
#define EXB_HTTP_REQUEST_HANDLER_H
struct exb_request_state;
typedef int (*exb_request_handler_func)(void *handler_state,
                                        struct exb_request_state *rqstate,
                                        int reason);
#endif
