#include "http_parse.h"
#include "http_request.h"
#include "http_server.h"
void cpb_request_repr(struct cpb_request_state *rqstate) {
    char *ibuff = rqstate->input_buffer;
    struct cpb_str_slice path = rqstate->path_s;
    struct cpb_str_slice method = rqstate->method_s;
    struct cpb_str_slice version = rqstate->version_s;
    struct cpb_str t1;
    struct cpb_str t2;
    struct cpb_str t3;
    struct cpb *cpb = rqstate->server->cpb;
    cpb_str_init_strlcpy(cpb, &t1, ibuff+path.index, path.len);
    cpb_str_init_strlcpy(cpb, &t2, ibuff+method.index, method.len);
    cpb_str_init_strlcpy(cpb, &t3, ibuff+version.index, version.len);

    
    fprintf(stderr,  "Request:\n"\
            "\tPATH:    '%s'\n"\
            "\tMETHOD:  '%s'\n"\
            "\tVERSION: '%s'\n", t1.str, t2.str, t3.str);
    fprintf(stderr,  "\tHeaders:\n");
    for (int i=0; i<rqstate->headers.len; i++) {
        struct cpb_http_header *h = rqstate->headers.headers + i;
        cpb_str_strlcpy(cpb, &t1, ibuff+h->key.index,   h->key.len);
        cpb_str_strlcpy(cpb, &t2, ibuff+h->value.index, h->value.len);

    fprintf(stderr,  "\t\tKey:   '%s'\n"\
                     "\t\tValue: '%s'\n", t1.str, t2.str);
    }
    cpb_str_deinit(rqstate->server->cpb, &t1);
    cpb_str_deinit(rqstate->server->cpb, &t2);
    cpb_str_deinit(rqstate->server->cpb, &t3);
}



