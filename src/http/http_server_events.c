#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include "../eloop.h"
#include "http_server.h"
#include "http_server_events.h"
#include "http_parse.h"

static void handle_http(struct cpb_event ev);
static void destroy_http(struct cpb_event ev);

struct cpb_event_handler_itable cpb_event_handler_http_itable = {
    .handle = handle_http,
    .destroy = destroy_http,
};

static void cpb_request_handle_http_error(struct cpb_request_state *rqstate) {
    //TODO should terminate connection not whole server
    abort();
}
static void cpb_request_handle_fatal_error(struct cpb_request_state *rqstate) {
    //TODO should terminate connection not whole server
    abort();
}

static void cpb_request_call_handler(struct cpb_request_state *rqstate, enum cpb_request_handler_reason reason) {
    rqstate->server->request_handler(rqstate, reason);
}

struct cpb_error cpb_request_on_bytes_read(struct cpb_request_state *rqstate, int index, int nbytes); //fwd

static struct cpb_error cpb_request_fork(struct cpb_request_state *rqstate) {
    struct cpb_server *s = rqstate->server;
    struct cpb_http_multiplexer *mp = cpb_server_get_multiplexer(s, rqstate->socket_fd);
    cpb_assert_h(mp && mp->state == CPB_MP_ACTIVE, "");
    
    struct cpb_request_state *forked_rqstate = cpb_server_new_rqstate(s, rqstate->socket_fd);
    
    
    cpb_assert_h(mp->creading == rqstate, "Tried to fork a request that is not the current one reading on socket");
    mp->creading = forked_rqstate;
    cpb_http_multiplexer_queue_response(mp, forked_rqstate);

    //Copy mistakenly read bytes from older request to the new one
    int bytes_copied = rqstate->input_buffer_len - rqstate->next_request_cursor;
    fprintf(stderr,
            "Server: Forked request. %d bytes copied from old request\n", bytes_copied);
    /*TODO: Deal with this once we have dynamic buffers*/
    cpb_assert_h(bytes_copied < cpb_request_input_buffer_size(forked_rqstate), "");
    memcpy(forked_rqstate->input_buffer, rqstate->input_buffer + rqstate->next_request_cursor, bytes_copied);
    forked_rqstate->input_buffer_len = bytes_copied;
    struct cpb_error err = cpb_request_on_bytes_read(forked_rqstate, 0, bytes_copied);
    if (err.error_code != CPB_OK) {
        return err;
    }
    

    struct cpb_event ev;
    cpb_event_http_init(&ev, forked_rqstate->socket_fd, CPB_HTTP_CONTINUE, forked_rqstate);
    cpb_eloop_append(s->eloop, ev);
    return cpb_make_error(CPB_OK);
}


struct cpb_error cpb_request_on_headers_read(struct cpb_request_state *rqstate) {
    struct cpb_error err  = cpb_request_http_parse(rqstate);
    if (err.error_code != CPB_OK) {
        cpb_request_handle_http_error(rqstate);
        /*TODO: IS IT ALWAYS A GOOD IDEA TO END THE CONNECTION HERE?*/
        return err;
        
    }
    int rv = cpb_request_http_check_validity(rqstate);
    if (rv != CPB_OK) {
        cpb_request_handle_http_error(rqstate);
        /*TODO: IS IT ALWAYS A GOOD IDEA TO END THE CONNECTION HERE?*/
        return cpb_make_error(rv);
    }
    if (rqstate->is_chunked) {
        rqstate->parse_chunk_cursor = rqstate->body_s.index; //first chunk
    }

    return cpb_make_error(CPB_OK);
}


//assumes rqstate->input_buffer_len is already adjusted for new bytes
struct cpb_error cpb_request_on_bytes_read(struct cpb_request_state *rqstate, int index, int nbytes) {
    struct cpb_error err = {0};
    dp_register_event(__FUNCTION__);
    rqstate->bytes_read += nbytes;
    int scan_idx = index - 3;
    scan_idx = scan_idx < 0 ? 0 : scan_idx;
    int scan_len = index + nbytes;
    
    fprintf(stderr, "READ %d BYTES, TOTAL %d BYTES\n", nbytes, rqstate->input_buffer_len);

    if (rqstate->istate == CPB_HTTP_I_ST_DONE) {
        err = cpb_make_error(CPB_INVALID_STATE_ERR);
        goto ret;
    }

    if (rqstate->istate == CPB_HTTP_I_ST_INIT)
        rqstate->istate = CPB_HTTP_I_ST_WAITING_FOR_HEADERS;
    
    if (rqstate->istate == CPB_HTTP_I_ST_WAITING_FOR_HEADERS )
    {
        if (cpb_str_has_crlfcrlf(rqstate->input_buffer, scan_idx, scan_len)) {
            err = cpb_request_on_headers_read(rqstate);
            if (err.error_code != CPB_OK) {
                goto ret;
            }
            cpb_request_call_handler(rqstate, CPB_HTTP_HANDLER_HEADERS);
        
            rqstate->istate = CPB_HTTP_I_ST_WAITING_FOR_BODY;
        }
    }
    if (rqstate->istate == CPB_HTTP_I_ST_WAITING_FOR_BODY ) 
    {
        cpb_assert_h(rqstate->pstate == CPB_HTTP_P_ST_DONE || rqstate->pstate == CPB_HTTP_P_ST_IN_CHUNKED_BODY, "");
        if (!cpb_request_has_body(rqstate)) {
            rqstate->istate = CPB_HTTP_I_ST_DONE;
        }
        else if (rqstate->is_chunked && (cpb_str_has_crlfcrlf(rqstate->input_buffer, scan_idx, scan_len))) {
            int rv = cpb_request_http_parse_chunked_encoding(rqstate);
            if (rv != CPB_OK) {
                err = cpb_make_error(rv);
                goto ret;
            }
            if (cpb_request_is_chunked_body_complete(rqstate)) {
                rqstate->istate = CPB_HTTP_I_ST_DONE;
            }
        }
        else if (cpb_request_body_bytes_read(rqstate) >= rqstate->content_length) {
            if (rqstate->body_handling == CPB_HTTP_B_BUFFER) {
                rqstate->istate = CPB_HTTP_I_ST_DONE;
                rqstate->next_request_cursor = rqstate->body_s.index + rqstate->content_length;
                int rv = cpb_str_strlappend(rqstate->server->cpb, &rqstate->body_decoded, rqstate->input_buffer + rqstate->body_s.index, rqstate->content_length);
                if (rv != CPB_OK) {
                    err = cpb_make_error(rv);
                    goto ret;
                }
            }
            else {
                cpb_assert_h(rqstate->body_handling == CPB_HTTP_B_DISCARD, "");
            }
            rqstate->istate = CPB_HTTP_I_ST_DONE;
        }
    }
    
    if (rqstate->istate == CPB_HTTP_I_ST_DONE) {
        cpb_assert_h(rqstate->pstate == CPB_HTTP_P_ST_DONE, "");

            if (cpb_request_has_body(rqstate)) {
                if (rqstate->body_handling == CPB_HTTP_B_BUFFER) {
                    cpb_request_call_handler(rqstate, CPB_HTTP_HANDLER_BODY);
                }
                else if (rqstate->body_handling != CPB_HTTP_B_DISCARD) {
                    cpb_assert_h(0, "");
                }
                    
                if (!rqstate->is_chunked) {
                    rqstate->next_request_cursor = rqstate->body_s.index + rqstate->content_length; 
                    //otherwise rqstate->next_request_cursor is set during chunked parsing
                }
            }
            else {
                rqstate->next_request_cursor = rqstate->body_s.index;
            }

        if (rqstate->is_persistent) {
            err = cpb_request_fork(rqstate);
            if (err.error_code != CPB_OK) {
                cpb_request_handle_fatal_error(rqstate);
                goto ret;
            }
            /*TODO: Destroy request after response is sent*/
        }
        else {
            //TODO: connection will be closed by handler after response is sent
            /*TODO: Destroy request after response is sent*/
        }
    }
    

    
ret:
    dp_end_event(__FUNCTION__);
    return err;
}



/*This is not the only source of bytes the request has, see also cpb_request_fork*/
struct cpb_error read_from_client(struct cpb_request_state *rqstate, int socket) {
    int avbytes = HTTP_INPUT_BUFFER_SIZE - rqstate->input_buffer_len - 1;
    int nbytes;
    struct cpb_error err = {0};
    dp_register_event(__FUNCTION__);
    again:
    nbytes = read(socket, rqstate->input_buffer + rqstate->input_buffer_len, avbytes);
    if (nbytes < 0) {
        if (!(errno == EWOULDBLOCK || errno == EAGAIN)) {
            err = cpb_make_error(CPB_READ_ERR);
            fprintf(stderr, "READ ERROR");
            goto ret;
        }
        else {
            err = cpb_make_error(CPB_OK);
            goto ret;
        }
    }
    else if (nbytes == 0) {
        struct cpb_event ev;
        cpb_event_http_init(&ev, socket, CPB_HTTP_CLOSE, rqstate);
        //TODO error handling, also we can directly deal with the event because cache is hot
        cpb_eloop_append(rqstate->server->eloop, ev); 
        fprintf(stderr, "EOF");
    }
    else {
        int idx = rqstate->input_buffer_len;
        rqstate->input_buffer_len += nbytes;
        err = cpb_request_on_bytes_read(rqstate, idx, nbytes);
        if (err.error_code != CPB_OK)
            goto ret;   
    }

    if (rqstate->istate != CPB_HTTP_I_ST_DONE) {
        if (nbytes != 0)
            goto again;
    }
    
    if (nbytes == 0) {
        /*TODO: This was closed by the client, check if it's valid*/
        /*For example if we are waiting for a request body and connection was closed prematurely*/
        rqstate->istate = CPB_HTTP_R_ST_DONE;
        cpb_server_close_connection(rqstate->server, rqstate->socket_fd);
    }
    ret:
    dp_end_event(__FUNCTION__);
    return err;
}




static void handle_http(struct cpb_event ev) {
    int socket_fd = ev.msg.arg1;
    int cmd  = ev.msg.arg2;
    struct cpb_request_state *rqstate = ev.msg.argp;
    if (cmd == CPB_HTTP_INIT || cmd == CPB_HTTP_CONTINUE || cmd == CPB_HTTP_READ) {
        read_from_client(rqstate, socket_fd);
    }
    else if (cmd == CPB_HTTP_CLOSE) {
        //Socket reached EOF
        cpb_server_close_connection(rqstate->server, socket_fd);
    }
    else if (cmd == CPB_HTTP_SEND) {
        if (rqstate->resp.state != CPB_HTTP_R_ST_DONE) {
            int rv = cpb_response_send(&rqstate->resp);
            if (rv != CPB_OK) {
                cpb_request_handle_fatal_error(rqstate);
                return;
            }
            if (rqstate->resp.state == CPB_HTTP_R_ST_DONE && !rqstate->is_persistent) {
                //TODO destroy
                cpb_server_close_connection(rqstate->server, rqstate->socket_fd);
            }
        }
    }
    else{
        cpb_assert_h(0, "invalid cmd");
    }
    return;
}
static void destroy_http(struct cpb_event ev) {
}

