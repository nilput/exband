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

static void cpb_request_call_handler(struct cpb_request_state *rqstate) {
    rqstate->server->request_handler(rqstate);
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
    /*
    Here we parsed the headers, and:
        There is no body:
            proceed
        There is a body:
            one of:
                We could've read the entire body
                we could've partially/read none of the body
            one of:
                We could be getting a chunked POST                        
                We could be getting a request with content length
    if it's a persistent connection:
        if there is no body or the body completed:
            Fork request into a new one and start parsing
    Otherwise:
        if there is a body:
            read it and call handler again if it didn't send a response yet
        close connection
        
    */

    if (cpb_request_has_body(rqstate)) {
        rqstate->istate = CPB_HTTP_I_ST_WAITING_FOR_BODY;
        if (rqstate->is_chunked) {
            rqstate->pstate = CPB_HTTP_P_ST_IN_CHUNKED_BODY;
        }
        else {
            cpb_assert_h(rqstate->headers.h_content_length_idx != -1 && rqstate->content_length >= 0, "");
            rqstate->pstate = CPB_HTTP_P_ST_DONE;
            if (cpb_request_body_bytes_read(rqstate) >= rqstate->content_length) {
                rqstate->istate = CPB_HTTP_I_ST_DONE;
                rqstate->next_request_cursor = rqstate->body_s.index + rqstate->content_length;
            }
            
        }
        
    }
    else {
        rqstate->pstate = CPB_HTTP_P_ST_DONE;
        rqstate->istate = CPB_HTTP_I_ST_DONE;
        rqstate->next_request_cursor = rqstate->body_s.index;
    }
    return cpb_make_error(CPB_OK);
}


//assumes rqstate->input_buffer_len is already adjusted for new bytes
struct cpb_error cpb_request_on_bytes_read(struct cpb_request_state *rqstate, int index, int nbytes) {
    struct cpb_error err = {0};
    int just_completed_headers = 0; //booleans
    int just_completed_body    = 0;
    int scan_idx = index - 3;
    scan_idx = scan_idx < 0 ? 0 : scan_idx;
    int scan_len = index + nbytes;
    
    fprintf(stderr, "READ %d BYTES, TOTAL %d BYTES\n", nbytes, rqstate->input_buffer_len);

    if (rqstate->istate == CPB_HTTP_I_ST_INIT)
        rqstate->istate = CPB_HTTP_I_ST_WAITING_FOR_HEADERS;
    
    if (rqstate->pstate == CPB_HTTP_P_ST_INIT ) 
    {
        if (cpb_str_has_crlfcrlf(rqstate->input_buffer, scan_idx, scan_len)) {
            err = cpb_request_on_headers_read(rqstate);
            if (err.error_code != CPB_OK)
                return err;
            just_completed_headers = 1;
            just_completed_body = rqstate->istate == CPB_HTTP_I_ST_DONE;
            if (rqstate->pstate == CPB_HTTP_P_ST_IN_CHUNKED_BODY)
                goto chunked;
        }
    }
    else if (rqstate->pstate == CPB_HTTP_P_ST_IN_CHUNKED_BODY ) 
    {
        if (cpb_str_has_crlfcrlf(rqstate->input_buffer, scan_idx, scan_len)) {
            int rv;
        chunked:
            rv = cpb_request_http_parse_chunked_encoding(rqstate);
            if (rv != CPB_OK) {
                return cpb_make_error(rv);
            }
            just_completed_body = cpb_request_is_chunked_body_complete(rqstate);
        }
    }
    else {
        if (rqstate->istate == CPB_HTTP_I_ST_WAITING_FOR_BODY) {
            cpb_assert_h(rqstate->pstate == CPB_HTTP_P_ST_DONE, "");
            if (cpb_request_body_bytes_read(rqstate) >= rqstate->content_length) {
                rqstate->istate = CPB_HTTP_I_ST_DONE;
                rqstate->next_request_cursor = rqstate->body_s.index + rqstate->content_length;
                /*TODO: call handler*/
            }

        }
        else {
            //TODO: ensure not possible
            //This will happen if we forked a request and a READ cmd was done before it, scheduling ths old one in the event loop
            fprintf(stderr, "read to wrong request");
            //cpb_server_fatal_error();
            cpb_assert_h(0, "");
        }
    }
    if (just_completed_headers || just_completed_body) {
        cpb_request_call_handler(rqstate);
    }

    return cpb_make_error(CPB_OK);
}


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


/*This is not the only source of bytes the request has, see also cpb_request_fork*/
struct cpb_error read_from_client(struct cpb_request_state *rqstate, int socket) {
    int avbytes = HTTP_INPUT_BUFFER_SIZE - rqstate->input_buffer_len - 1;
    int nbytes;
    struct cpb_error err = {0};
    again:
    nbytes = read(socket, rqstate->input_buffer + rqstate->input_buffer_len, avbytes);
    if (nbytes < 0) {
        if (!(errno == EWOULDBLOCK || errno == EAGAIN)) {
            err = cpb_make_error(CPB_READ_ERR);
            fprintf(stderr, "READ ERROR");
            return err;
        }
        else {
            return cpb_make_error(CPB_OK);
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
            return err;   
    }

    if (rqstate->istate != CPB_HTTP_I_ST_DONE && nbytes != 0) {
        goto again;
    }
    else if (rqstate->is_persistent) {
        err = cpb_request_fork(rqstate);
        if (err.error_code != CPB_OK) {
            cpb_request_handle_fatal_error(rqstate);
        }
        return err;
        /*TODO: Destroy request after response is sent*/
    }
    else {
        if (nbytes == 0) {
            /*TODO: This was closed by the client, check if it's valid*/
            /*For example if we are waiting for a request body and connection was closed prematurely*/
            rqstate->istate = CPB_HTTP_R_ST_DONE;
            cpb_server_close_connection(rqstate->server, rqstate->socket_fd);
        }
        //TODO: connection will be closed by handler after response is sent
        /*TODO: Destroy request after response is sent*/
    }
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
    else if (cmd == CPB_HTTP_SEND && rqstate->resp.state != CPB_HTTP_R_ST_DONE) {
        int rv = cpb_response_send(&rqstate->resp);
        if (rv != CPB_OK) {
            cpb_request_handle_fatal_error(rqstate);
            return;
        }
    }

    else{
        cpb_assert_h(0, "invalid cmd");
    }
    return;
}
static void destroy_http(struct cpb_event ev) {
}

