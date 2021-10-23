#include "http_response.h"
#include "http_request.h"
#include "http_server.h"
#include <unistd.h>
#include <errno.h>

/*ensure body buffer fits capacity, or try to grow*/
int exb_response_body_buffer_ensure(struct exb_request_state *rqstate, size_t cap) {
    struct exb_response_state *rsp = &rqstate->resp;
    
    cap = cap + rsp->body_begin_index;
    if (cap > rsp->output_buffer_cap) {
        struct exb_error err;
        char *new_buff;
        int new_sz;
        err = exb_evloop_realloc_buffer(rqstate->evloop, rsp->output_buffer, cap, &new_buff, &new_sz);
        if (err.error_code)
            return err.error_code;
        rsp->output_buffer = new_buff;
        rsp->output_buffer_cap = new_sz;
    }
    return EXB_OK;
}



int exb_response_append_body(struct exb_request_state *rqstate, char *s, int len)
{
    return exb_response_append_body_i(rqstate, s, len);
}
int exb_response_append_body_cstr(struct exb_request_state *rqstate, char *s)
{
    return exb_response_append_body_cstr_i(rqstate, s);
}


//Takes ownership of both name and value
int exb_response_add_header(struct exb_request_state *rqstate, struct exb_str *name, struct exb_str *value) {
    struct exb_response_state *rsp = &rqstate->resp;
    if (rsp->headers.len + 1 >= EXB_HTTP_RESPONSE_HEADER_MAX) {
        exb_str_deinit(rqstate->server->exb, name);
        exb_str_deinit(rqstate->server->exb, value);
        return EXB_OUT_OF_RANGE_ERR;
    }
    rsp->headers.headers[rsp->headers.len].key = *name;
    rsp->headers.headers[rsp->headers.len].value = *value;
    rsp->headers.len++;

    rsp->headers_bytes += name->len + 2 /*": "*/ + value->len + 2 /*crlf*/;

    return EXB_OK;
}

//Takes ownership of both name and value
int exb_response_set_header(struct exb_request_state *rqstate, struct exb_str *name, struct exb_str *value) {
    struct exb_response_state *rsp = &rqstate->resp;
    
    int idx = exb_response_get_header_index(rqstate, name->str, name->len);
    if (idx != -1) {
        struct exb_str *old_value = &rsp->headers.headers[idx].value;
        exb_str_deinit(rqstate->server->exb, old_value);
        exb_str_deinit(rqstate->server->exb, name);
        rsp->headers.headers[idx].value = *value;
        return EXB_OK;
    }
    return exb_response_add_header(rqstate, name, value);
}

int exb_response_set_header_c(struct exb_request_state *rqstate, char *name, char *value) {
    struct exb_str sname, svalue;
    int rv;
    if ((rv = exb_str_init_strcpy(rqstate->server->exb, &sname, name)) != EXB_OK) {
        return rv;
    }
    if ((rv = exb_str_init_strcpy(rqstate->server->exb, &svalue, name)) != EXB_OK) {
        exb_str_deinit(rqstate->server->exb, &sname);
        return rv;
    }
    return exb_response_set_header(rqstate, &sname, &svalue);
}
int exb_response_add_header_c(struct exb_request_state *rqstate, char *name, char *value) {
    struct exb_str sname, svalue;
    int rv;
    if ((rv = exb_str_init_strcpy(rqstate->server->exb, &sname, name)) != EXB_OK) {
        return rv;
    }
    if ((rv = exb_str_init_strcpy(rqstate->server->exb, &svalue, name)) != EXB_OK) {
        exb_str_deinit(rqstate->server->exb, &sname);
        return rv;
    }
    return exb_response_add_header(rqstate, &sname, &svalue);
}

int exb_response_redirect_and_end(struct exb_request_state *rqstate, int status_code, const char *location) {
    struct exb_response_state *rsp = &rqstate->resp;
    
    struct exb_str name, value;
    exb_str_init_const_str(&name, "Location");
    int rv =  exb_str_init_strcpy(rqstate->server->exb, &value, location);
    if (rv != EXB_OK)
        return rv;
    rv = exb_response_set_header(rqstate, &name, &value);
    if (rv != EXB_OK) {
        exb_str_deinit(rqstate->server->exb, &value);
        return rv;
    }
    exb_response_set_status_code(rqstate, status_code);
    return exb_response_end(rqstate);
}
