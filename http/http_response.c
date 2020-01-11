#include "http_response.h"
#include "http_request.h"
#include "http_server.h"
#include <unistd.h>
#include <errno.h>
int cpb_response_send(struct cpb_response_state *rsp) {
    if (rsp->state != CPB_HTTP_R_ST_SENDING)
        return CPB_INVALID_STATE_ERR;
    int header_bytes = rsp->headers_buff_len;
    int body_bytes   = rsp->headers_buff_len;
    int total_bytes  = header_bytes + body_bytes;
    if (rsp->written_bytes < total_bytes) {
        if (rsp->written_bytes < header_bytes) {
            ssize_t written = write(rsp->req_state->socket_fd,
                                    rsp->headers_buff + rsp->written_bytes,
                                    rsp->headers_buff_len - rsp->written_bytes);
            if (written == -1) {
                if (errno != EWOULDBLOCK && errno != EAGAIN) {
                    return CPB_WRITE_ERR;
                }
            }
            else {
                rsp->written_bytes += written;
            }
        }
        if (rsp->written_bytes >= header_bytes) {
            ssize_t written = write(rsp->req_state->socket_fd,
                                    rsp->output_buff + (rsp->written_bytes - header_bytes),
                                    total_bytes - rsp->written_bytes);
            if (written == -1) {
                if (errno != EWOULDBLOCK && errno != EAGAIN) {
                    return CPB_WRITE_ERR;
                }
            }
            else {
                rsp->written_bytes += written;
            }
        }
    }
    if (rsp->written_bytes >= total_bytes) {
        cpb_assert_h(rsp->written_bytes == total_bytes, "wrote more than expected");
        rsp->state = CPB_HTTP_R_ST_DONE;
    }
    return CPB_OK;
}
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

int cpb_response_end(struct cpb_response_state *rsp) {
    if (rsp->state == CPB_HTTP_R_ST_SENDING || rsp->state == CPB_HTTP_R_ST_DONE) {
        return CPB_INVALID_STATE_ERR;
    }
    int rv;
    
    struct cpb_str name;
    cpb_str_init_const_str(rsp->req_state->server->cpb, &name, "Content-Length");
    int body_len = rsp->output_buff_len;
    struct cpb_str body_len_str;
    cpb_str_init(rsp->req_state->server->cpb, &body_len_str);
    cpb_str_itoa(rsp->req_state->server->cpb, &body_len_str, body_len);
    rv = cpb_response_set_header(rsp, &name, &body_len_str); //owns body_len_str
    if (rv != CPB_OK) {
        return rv;
    }

    //TODO: Support persistent connections
    //
    //cpb_str_init_const_str(rsp->req_state->server->cpb, &name, "Connection");
    //cpb_str_init_const_str(rsp->req_state->server->cpb, &value, "close");
    //rv = cpb_response_set_header(rsp, &name, &value);
    if (rv != CPB_OK) {
        return rv;
    }
        
    rv = cpb_response_prepare_headers(rsp);
    if (rv != CPB_OK)
        return rv;

    rsp->state = CPB_HTTP_R_ST_SENDING;

    
    //TODO: Why not do that directly
    struct cpb_http_multiplexer *m = cpb_server_get_multiplexer(rsp->req_state->server, rsp->req_state->socket_fd);
    if (m->next_response == rsp->req_state) {
        //TODO: will it be an issue if this gets scheduled twice? 
        //  [Whatever event we are on] -> we schedule this here
        //  [select() event] -> schedules this too
        //  [First  time scheduled]
        //  [Second time scheduled]
        struct cpb_event ev;
        cpb_event_http_init(&ev, rsp->req_state->socket_fd, CPB_HTTP_SEND, rsp->req_state);
        rv = cpb_eloop_append(rsp->req_state->server->eloop, ev);
    }

    return rv;
}
