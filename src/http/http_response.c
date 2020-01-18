#include "http_response.h"
#include "http_request.h"
#include "http_server.h"
#include <unistd.h>
#include <errno.h>


//Takes ownership of both name and value
int cpb_response_set_header(struct cpb_response_state *rsp, struct cpb_str *name, struct cpb_str *value) {
    int idx = cpb_response_get_header_index(rsp, name->str, name->len);
    if (idx != -1) {
        struct cpb_str *old_value = &rsp->headers.headers[idx].value;
        cpb_str_deinit(rsp->req_state->server->cpb, old_value);
        cpb_str_deinit(rsp->req_state->server->cpb, name);
        rsp->headers.headers[idx].value = *value;
        return CPB_OK;
    }
    if (rsp->headers.len + 1 >= CPB_HTTP_RESPONSE_HEADER_MAX) {
        cpb_str_deinit(rsp->req_state->server->cpb, name);
        cpb_str_deinit(rsp->req_state->server->cpb, value);
        return CPB_OUT_OF_RANGE_ERR;
    }
    rsp->headers.headers[rsp->headers.len].key = *name;
    rsp->headers.headers[rsp->headers.len].value = *value;
    rsp->headers.len++;
    return CPB_OK;
}

int cpb_response_redirect_and_end(struct cpb_response_state *rsp, int status_code, const char *location) {
    struct cpb_str name, value;
    cpb_str_init_const_str(&name, "Location");
    int rv =  cpb_str_init_strcpy(rsp->req_state->server->cpb, &value, location);
    if (rv != CPB_OK)
        return rv;
    rv = cpb_response_set_header(rsp, &name, &value);
    if (rv != CPB_OK) {
        cpb_str_deinit(rsp->req_state->server->cpb, &value);
        return rv;
    }
    cpb_response_set_status_code(rsp, status_code);
    return cpb_response_end(rsp);
}
