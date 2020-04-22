#include "http_parse.h"
#include "http_request.h"
#include "http_server.h"
void exb_request_repr(struct exb_request_state *rqstate) {
    char *ibuff = rqstate->input_buffer;
    struct exb_str_slice path = rqstate->path_s;
    struct exb_str_slice method = rqstate->method_s;
    struct exb_str_slice version = rqstate->version_s;
    struct exb_str t1;
    struct exb_str t2;
    struct exb_str t3;
    struct exb *exb = rqstate->server->exb;
    exb_str_init_strlcpy(exb, &t1, ibuff+path.index, path.len);
    exb_str_init_strlcpy(exb, &t2, ibuff+method.index, method.len);
    exb_str_init_strlcpy(exb, &t3, ibuff+version.index, version.len);
    fprintf(stderr,  "Request:\n"\
            "\tPATH:    '%s'\n"\
            "\tMETHOD:  '%s'\n"\
            "\tVERSION: '%s'\n", t1.str, t2.str, t3.str);
    fprintf(stderr,  "\tHeaders:\n");
    for (int i=0; i<rqstate->headers.len; i++) {
        struct exb_http_header *h = rqstate->headers.headers + i;
        exb_str_strlcpy(exb, &t1, ibuff+h->key.index,   h->key.len);
        exb_str_strlcpy(exb, &t2, ibuff+h->value.index, h->value.len);

    fprintf(stderr,  "\t\tKey:   '%s'\n"\
                     "\t\tValue: '%s'\n", t1.str, t2.str);
    }
    exb_str_deinit(rqstate->server->exb, &t1);
    exb_str_deinit(rqstate->server->exb, &t2);
    exb_str_deinit(rqstate->server->exb, &t3);
}
