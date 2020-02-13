#include "http_response.h"
#include "http_request.h"
#include "http_server.h"
#include <unistd.h>
#include <errno.h>

int cpb_response_body_buffer_ensure(struct cpb_request_state *rqstate, size_t cap) {
    struct cpb_response_state *rsp = &rqstate->resp;
    
    cap = cap + rsp->body_begin_index;
    if (cap > rsp->output_buffer_cap) {
        struct cpb_error err;
        char *new_buff;
        int new_sz;
        err = cpb_eloop_realloc_buffer(rqstate->eloop, rsp->output_buffer, cap, &new_buff, &new_sz);
        if (err.error_code)
            return err.error_code;
        rsp->output_buffer = new_buff;
        rsp->output_buffer_cap = new_sz;
    }
    return CPB_OK;
}

int cpb_response_append_body_cstr(struct cpb_request_state *rqstate, char *s) {
    return cpb_response_append_body(rqstate, s, strlen(s));
}



//Takes ownership of both name and value
int cpb_response_add_header(struct cpb_request_state *rqstate, struct cpb_str *name, struct cpb_str *value) {
    struct cpb_response_state *rsp = &rqstate->resp;
    if (rsp->headers.len + 1 >= CPB_HTTP_RESPONSE_HEADER_MAX) {
        cpb_str_deinit(rqstate->server->cpb, name);
        cpb_str_deinit(rqstate->server->cpb, value);
        return CPB_OUT_OF_RANGE_ERR;
    }
    rsp->headers.headers[rsp->headers.len].key = *name;
    rsp->headers.headers[rsp->headers.len].value = *value;
    rsp->headers.len++;

    rsp->headers_bytes += name->len + 2 /*": "*/ + value->len + 2 /*crlf*/;

    return CPB_OK;
}

//Takes ownership of both name and value
int cpb_response_set_header(struct cpb_request_state *rqstate, struct cpb_str *name, struct cpb_str *value) {
    struct cpb_response_state *rsp = &rqstate->resp;
    
    int idx = cpb_response_get_header_index(rqstate, name->str, name->len);
    if (idx != -1) {
        struct cpb_str *old_value = &rsp->headers.headers[idx].value;
        cpb_str_deinit(rqstate->server->cpb, old_value);
        cpb_str_deinit(rqstate->server->cpb, name);
        rsp->headers.headers[idx].value = *value;
        return CPB_OK;
    }
    return cpb_response_add_header(rqstate, name, value);
}

int cpb_response_set_header_c(struct cpb_request_state *rqstate, char *name, char *value) {
    struct cpb_str sname, svalue;
    int rv;
    if ((rv = cpb_str_init_strcpy(rqstate->server->cpb, &sname, name)) != CPB_OK) {
        return rv;
    }
    if ((rv = cpb_str_init_strcpy(rqstate->server->cpb, &svalue, name)) != CPB_OK) {
        cpb_str_deinit(rqstate->server->cpb, &sname);
        return rv;
    }
    return cpb_response_set_header(rqstate, &sname, &svalue);
}
int cpb_response_add_header_c(struct cpb_request_state *rqstate, char *name, char *value) {
    struct cpb_str sname, svalue;
    int rv;
    if ((rv = cpb_str_init_strcpy(rqstate->server->cpb, &sname, name)) != CPB_OK) {
        return rv;
    }
    if ((rv = cpb_str_init_strcpy(rqstate->server->cpb, &svalue, name)) != CPB_OK) {
        cpb_str_deinit(rqstate->server->cpb, &sname);
        return rv;
    }
    return cpb_response_add_header(rqstate, &sname, &svalue);
}

int cpb_response_redirect_and_end(struct cpb_request_state *rqstate, int status_code, const char *location) {
    struct cpb_response_state *rsp = &rqstate->resp;
    
    struct cpb_str name, value;
    cpb_str_init_const_str(&name, "Location");
    int rv =  cpb_str_init_strcpy(rqstate->server->cpb, &value, location);
    if (rv != CPB_OK)
        return rv;
    rv = cpb_response_set_header(rqstate, &name, &value);
    if (rv != CPB_OK) {
        cpb_str_deinit(rqstate->server->cpb, &value);
        return rv;
    }
    cpb_response_set_status_code(rqstate, status_code);
    return cpb_response_end(rqstate);
}
