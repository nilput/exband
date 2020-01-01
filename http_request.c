#include "http_parse.h"
#include "http_request.h"
#include "server.h"
void cpb_request_repr(struct cpb_request_state *rstate) {
    char *ibuff = rstate->input_buffer;
    struct cpb_str_slice path = rstate->path_s;
    struct cpb_str_slice method = rstate->method_s;
    struct cpb_str_slice version = rstate->version_s;
    struct cpb_str t1;
    struct cpb_str t2;
    struct cpb_str t3;
    cpb_str_init_strlcpy(rstate->server->cpb, &t1, ibuff+path.index, path.len);
    cpb_str_init_strlcpy(rstate->server->cpb, &t2, ibuff+method.index, method.len);
    cpb_str_init_strlcpy(rstate->server->cpb, &t3, ibuff+version.index, version.len);

    printf( "Parse\n"\
            "PATH: '%s'\n"\
            "METHOD: '%s'\n"\
            "VERSION: '%s'\n", t1.str, t2.str, t3.str);
    cpb_str_deinit(rstate->server->cpb, &t1);
    cpb_str_deinit(rstate->server->cpb, &t2);
    cpb_str_deinit(rstate->server->cpb, &t3);
}