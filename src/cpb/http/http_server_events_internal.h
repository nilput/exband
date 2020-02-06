#ifndef CPB_HTTP_SERVER_EVENTS_INTERNAL
#define CPB_HTTP_SERVER_EVENTS_INTERNAL
#include <unistd.h>
#include <errno.h>
#include "http_server_events.h"
#include "http_server_internal.h"
#include "http_request.h"
#include "http_response.h"
#include "http_parse.h"

static struct cpb_error cpb_request_on_bytes_read(struct cpb_request_state *rqstate, int index, int nbytes);
static struct cpb_error cpb_response_on_bytes_written(struct cpb_request_state *rqstate, int index, int nbytes);
enum cpb_event_http_cmd {
    CPB_HTTP_READ, /*ask it to read, .argp : rqstate*/
    CPB_HTTP_SEND, /*ask it to write, .argp : rqstate*/
    CPB_HTTP_DID_READ,  /*inform about async read result, .argp : rqstate*/
    CPB_HTTP_DID_WRITE, /*inform about async write result, .argp : rqstate*/
    CPB_HTTP_WRITE_IO_ERROR, /*inform about error during async read/write, .argp : rqstate*/
    CPB_HTTP_READ_IO_ERROR,
    CPB_HTTP_INPUT_BUFFER_FULL, /*inform about a full buffer during async read, .argp : rqstate*/
    CPB_HTTP_CLIENT_CLOSED, //.argp : rqstate
    CPB_HTTP_CANCEL, /*.argp: http server, .arg1: socket*/
};

static int cpb_event_http_init(struct cpb_server *s, struct cpb_event *ev, int cmd, void *object, int arg);

static void cpb_request_on_request_done(struct cpb_request_state *rqstate);

static void cpb_request_handle_http_error(struct cpb_request_state *rqstate) {
    cpb_server_cancel_requests(rqstate->server, rqstate->socket_fd);
}


static void cpb_request_handle_fatal_error(struct cpb_request_state *rqstate) {
    //TODO should terminate connection not whole server
    abort();
}

static void cpb_request_handle_socket_error(struct cpb_request_state *rqstate) {
    if (rqstate->is_cancelled)
        return;
    cpb_server_cancel_requests(rqstate->server, rqstate->socket_fd);
}

static void cpb_request_call_handler(struct cpb_request_state *rqstate, enum cpb_request_handler_reason reason) {
    rqstate->server->request_handler(rqstate, reason);
}

static void cpb_request_lifetime(struct cpb_http_multiplexer *mp, struct cpb_request_state *rqstate) {
    if (!rqstate->is_read_scheduled && !rqstate->is_send_scheduled ) {
        //FIXME: if cancelled, are we sure no one else has a reference to it?
        if (rqstate->istate == CPB_HTTP_I_ST_DONE || rqstate->is_cancelled) {
            if (!rqstate->is_cancelled)
                cpb_assert_h(mp->creading != rqstate && mp->next_response != rqstate, "");
            cpb_server_destroy_rqstate(rqstate->server, mp->eloop, rqstate);
            RQSTATE_EVENT(stderr, "lifetime check for rqstate %p destroyed it\n", rqstate);
        }
    }
    else {
        RQSTATE_EVENT(stderr, "lifetime check for rqstate %p, "
                "didnt destroy it, because one of:"
                " state not done? %d read_scheduled? %d write_scheduled? %d\n", rqstate,
                                                                                rqstate->istate != CPB_HTTP_I_ST_DONE,
                                                                                rqstate->is_read_scheduled,
                                                                                rqstate->is_send_scheduled);
    }
}


static void cpb_request_on_request_done(struct cpb_request_state *rqstate) {
    cpb_assert_h(!rqstate->is_read_scheduled, "");
    cpb_assert_h(rqstate->istate == CPB_HTTP_I_ST_DONE || rqstate->is_cancelled, "");
    struct cpb_http_multiplexer *mp = cpb_server_get_multiplexer_i(rqstate->server, rqstate->socket_fd);
    cpb_request_lifetime(mp, rqstate);
}

static void cpb_request_on_response_done(struct cpb_request_state *rqstate) {
    cpb_assert_h(!rqstate->is_send_scheduled, "");
    cpb_assert_h(rqstate->resp.state == CPB_HTTP_R_ST_DONE, "");
    struct cpb_http_multiplexer *mp = cpb_server_get_multiplexer_i(rqstate->server, rqstate->socket_fd);
    cpb_assert_h(mp->next_response == rqstate, "");
    cpb_http_multiplexer_pop_response(mp);
    cpb_request_lifetime(mp, rqstate);
}


static struct cpb_error cpb_request_on_headers_read(struct cpb_request_state *rqstate) {
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
static struct cpb_error cpb_request_fork(struct cpb_request_state *rqstate) {

    struct cpb_error err;
    struct cpb_server *s = rqstate->server;
    struct cpb_http_multiplexer *mp = cpb_server_get_multiplexer_i(s, rqstate->socket_fd);
    if (mp->state != CPB_MP_ACTIVE)
        return cpb_make_error(CPB_SOCKET_ERR);
    cpb_assert_h(mp && mp->state == CPB_MP_ACTIVE, "");
    
    struct cpb_request_state *forked_rqstate = cpb_server_new_rqstate(s, mp->eloop, rqstate->socket_fd);
    cpb_assert_h(mp->creading == rqstate, "Tried to fork a request that is not the current one reading on socket");

    //Copy mistakenly read bytes from older request to the new one

    int bytes_copied = rqstate->input_buffer_len - rqstate->next_request_cursor;

    RQSTATE_EVENT(stderr, "Forking %p to %p with %d leftover bytes\n", rqstate, forked_rqstate, bytes_copied);

    int rv;
    if ((rv = cpb_request_input_buffer_ensure_cap(forked_rqstate, bytes_copied + 1)) != CPB_OK) {
        return cpb_make_error(rv);
    }
    
    cpb_assert_h(bytes_copied < cpb_request_input_buffer_size(forked_rqstate), "");
    memcpy(forked_rqstate->input_buffer, rqstate->input_buffer + rqstate->next_request_cursor, bytes_copied);
    err = cpb_request_on_bytes_read(forked_rqstate, 0, bytes_copied);
    if (err.error_code != CPB_OK) {
        cpb_server_destroy_rqstate(s, mp->eloop, forked_rqstate);
        cpb_server_cancel_requests(s, rqstate->socket_fd);
        goto ret;
    }
    
    cpb_assert_h(!rqstate->is_forked, "");
    rqstate->is_forked = 1;
    cpb_assert_h(!rqstate->is_read_scheduled, "");
    mp->creading = forked_rqstate;
    cpb_http_multiplexer_queue_response(mp, forked_rqstate);
    //RQSTATE_EVENT(stderr, "Scheduled rqstate %p to be read socket %d, because we just forked it\n",  forked_rqstate, forked_rqstate->socket_fd)
    err = cpb_make_error(CPB_OK);
ret:

    return err;
}
static struct cpb_error cpb_request_on_bytes_read(struct cpb_request_state *rqstate, int index, int nbytes) {
    struct cpb_error err = {0};

    cpb_assert_h(index == rqstate->input_buffer_len, "");
    rqstate->input_buffer_len += nbytes;
    rqstate->bytes_read += nbytes;
    int scan_idx = index - 3;
    scan_idx = scan_idx < 0 ? 0 : scan_idx;
    int scan_len = index + nbytes;
    
    //fprintf(stderr, "READ %d BYTES, TOTAL %d BYTES\n", nbytes, rqstate->input_buffer_len);

    if (rqstate->istate == CPB_HTTP_I_ST_DONE || rqstate->istate == CPB_HTTP_I_ST_DEAD) {
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
            rqstate->istate = CPB_HTTP_I_ST_WAITING_FOR_BODY;
            cpb_request_call_handler(rqstate, CPB_HTTP_HANDLER_HEADERS);
        }
    }
    if (rqstate->istate == CPB_HTTP_I_ST_WAITING_FOR_BODY ) 
    {
        cpb_assert_h(rqstate->pstate == CPB_HTTP_P_ST_DONE || rqstate->pstate == CPB_HTTP_P_ST_IN_CHUNKED_BODY, "");
        if (!cpb_request_has_body(rqstate)) {
            rqstate->istate = CPB_HTTP_I_ST_DONE;
            RQSTATE_EVENT(stderr, "marked request %p as done, because it has nobody\n", rqstate);
        }
        else if (rqstate->is_chunked && (cpb_str_has_crlfcrlf(rqstate->input_buffer, scan_idx, scan_len))) {
            int rv = cpb_request_http_parse_chunked_encoding(rqstate);
            if (rv != CPB_OK) {
                err = cpb_make_error(rv);
                goto ret;
            }
            if (cpb_request_is_chunked_body_complete(rqstate)) {
                rqstate->istate = CPB_HTTP_I_ST_DONE;
                RQSTATE_EVENT(stderr, "marked request %p as done, because its chunked body completed\n", rqstate);
            }
        }
        else if (cpb_request_body_bytes_read(rqstate) >= rqstate->content_length) {
            if (rqstate->body_handling == CPB_HTTP_B_BUFFER) {
                rqstate->istate = CPB_HTTP_I_ST_DONE;
                RQSTATE_EVENT(stderr, "marked request %p as done, because its body completed\n", rqstate);
                rqstate->next_request_cursor = rqstate->body_s.index + rqstate->content_length;
                cpb_assert_h(cpb_str_is_const(&rqstate->body_decoded), "");
                rqstate->body_decoded = cpb_str_slice_to_const_str(rqstate->body_s, rqstate->input_buffer);
                #if 0
                int rv = cpb_str_strlappend(rqstate->server->cpb, &rqstate->body_decoded, rqstate->input_buffer + rqstate->body_s.index, rqstate->content_length);
                if (rv != CPB_OK) {
                    err = cpb_make_error(rv);
                    goto ret;
                }
                #endif
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
                cpb_request_handle_socket_error(rqstate);
                goto ret;
            }
        }
        
        cpb_request_on_request_done(rqstate);
    }
   
ret:

    return err;
}
//assumes rqstate->input_buffer_len WAS NOT adjusted for new bytes

/*assumes rsp->written_bytes WAS NOT adjusted for new bytes*/

static struct cpb_error cpb_response_on_bytes_written(struct cpb_request_state *rqstate, int index, int nbytes) {
    struct cpb_response_state *rsp = cpb_request_get_response(rqstate);

    cpb_assert_h(index == rsp->written_bytes, "");
    rsp->written_bytes += nbytes;
    int total_bytes  = rsp->status_len + rsp->headers_len + rsp->body_len;
    if (rsp->written_bytes >= total_bytes) {
        cpb_assert_h(rsp->written_bytes == total_bytes, "wrote more than expected");
        rsp->state = CPB_HTTP_R_ST_DONE;
    }

    return cpb_make_error(CPB_OK);
}

static void cpb_request_async_read_from_client_runner(struct cpb_thread *thread, struct cpb_task *task) {
    struct cpb_request_state *rqstate = task->msg.u.iip.argp;
    int socket = rqstate->socket_fd;
    int avbytes = cpb_request_input_buffer_size(rqstate) - rqstate->input_buffer_len - 1;
    int nbytes;
    
    int err = CPB_OK;
    int client_closed = 0;

    int now_read_bytes = rqstate->bytes_read;

    struct cpb_event ev;
    int read_bytes;

    nbytes = read(socket, rqstate->input_buffer + rqstate->input_buffer_len, avbytes);

    if (nbytes < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            err = CPB_OK;
            RQSTATE_EVENT(stderr, "async read to rqstate %p, would block\n", rqstate, rqstate->socket_fd);
        }
        else {
            err = CPB_READ_ERR;
        }
    }
    else if (nbytes == 0) {
        if (avbytes == 0) {
            err = CPB_BUFFER_FULL_ERR;
        }
        else {
            client_closed = 1;
        }
    }
    else {
        avbytes -= nbytes;
        now_read_bytes += nbytes;
    }

    /*TODO: handle error*/
    /*TODO: provide more details than what can fit in an event*/
    
    read_bytes = now_read_bytes - rqstate->bytes_read;

    if (err != CPB_OK) {
        if (err == CPB_BUFFER_FULL_ERR) {
            //we cannot do reallocation here, eloop_* functions are not thread safe
            cpb_event_http_init(rqstate->server, &ev, CPB_HTTP_INPUT_BUFFER_FULL, rqstate, read_bytes);
            RQSTATE_EVENT(stderr, "async read to rqstate %p, socket %d, with %d bytes, added buffer full event\n", rqstate, rqstate->socket_fd, read_bytes);
        }
        else {
            cpb_event_http_init(rqstate->server, &ev, CPB_HTTP_READ_IO_ERROR, rqstate, read_bytes);
            RQSTATE_EVENT(stderr, "async read to rqstate %p, socket %d, with %d bytes, added io error event\n", rqstate, rqstate->socket_fd, read_bytes);
        }
        
        int perr = cpb_eloop_ts_append(rqstate->eloop, ev);
        if (perr != CPB_OK) {
            /*we cannot afford to have this fail*/
            cpb_request_handle_fatal_error(rqstate);
        }
    }
    else {
        //even if it's zero we need to send this to mark it as not scheduled
        cpb_event_http_init(rqstate->server, &ev, CPB_HTTP_DID_READ, rqstate, read_bytes);
        int perr = cpb_eloop_ts_append(rqstate->eloop, ev);
        if (perr != CPB_OK) {
            /*we cannot afford to have this fail*/
            cpb_request_handle_fatal_error(rqstate);
        }
        RQSTATE_EVENT(stderr, "async read to rqstate %p, socket %d, with %d bytes, added read event\n", rqstate, rqstate->socket_fd, read_bytes);
        
        if (client_closed) {
            cpb_event_http_init(rqstate->server, &ev, CPB_HTTP_CLIENT_CLOSED, rqstate, read_bytes);
            perr = cpb_eloop_ts_append(rqstate->eloop, ev);
            if (err != CPB_OK) {
                /*we cannot afford to have this fail*/
                cpb_request_handle_fatal_error(rqstate);
            }
            RQSTATE_EVENT(stderr, "async read to rqstate %p, socket %d, with %d bytes client closed\n", rqstate, rqstate->socket_fd, read_bytes);
        }
    }
    RQSTATE_EVENT(stderr, "Did async read to rqstate %p, socket %d, with %d bytes\n", rqstate, rqstate->socket_fd, read_bytes);
ret:    

    return;
}

static void cpb_request_async_write_runner(struct cpb_thread *thread, struct cpb_task *task) {

    struct cpb_request_state *rqstate = task->msg.u.iip.argp;
    struct cpb_response_state *rsp = &rqstate->resp;
    int rv = CPB_OK;
    int details = 0;
    int total_bytes  = rsp->status_len + rsp->headers_len + rsp->body_len;

    int current_written_bytes = rsp->written_bytes;
    struct cpb_event ev;

    if (current_written_bytes < total_bytes) {
        size_t offset =  rqstate->resp.status_begin_index + current_written_bytes;
        int sum = 0;
        dp_useless(sum);

        ssize_t written = write(rqstate->socket_fd,
                                rqstate->resp.output_buffer + offset,
                                total_bytes - current_written_bytes);
    

        if (written == -1) {
            if (errno != EWOULDBLOCK && errno != EAGAIN) {
                rv =  CPB_WRITE_ERR;
                details = errno;
                goto ret;
            }
        }
        else {
            current_written_bytes += written;
        }
    }
ret:
    if (rv != CPB_OK) {
        cpb_event_http_init(rqstate->server, &ev, CPB_HTTP_WRITE_IO_ERROR, rqstate, current_written_bytes - rsp->written_bytes);
    }
    else {
        cpb_event_http_init(rqstate->server, &ev, CPB_HTTP_DID_WRITE, rqstate, current_written_bytes - rsp->written_bytes);
    }
    int err = cpb_eloop_ts_append(rqstate->eloop, ev);
    if (err != CPB_OK) {
        /*we cannot afford to have this fail*/
        cpb_request_handle_fatal_error(rqstate);
    }

    return;
}
static void cpb_request_async_read_from_client_runner(struct cpb_thread *thread, struct cpb_task *task);
static struct cpb_error cpb_request_async_read_from_client(struct cpb_request_state *rqstate) {

    struct cpb_threadpool *tp = rqstate->eloop->threadpool;
    struct cpb_task task;
    task.err = cpb_make_error(CPB_OK);
    task.run = cpb_request_async_read_from_client_runner;
    task.msg.u.iip.arg1 = 0;
    task.msg.u.iip.arg2 = 0;
    task.msg.u.iip.argp = rqstate;
    int rv = cpb_threadpool_push_task(tp, task);

    return cpb_make_error(rv);
}

static struct cpb_error cpb_response_async_write(struct cpb_request_state *rqstate) {
    struct cpb_response_state *rsp = &rqstate->resp;
    int rv = CPB_OK;
    if (rsp->state != CPB_HTTP_R_ST_SENDING) {
        rv = CPB_INVALID_STATE_ERR;
        goto ret;
    }
    struct cpb_threadpool *tp = rqstate->eloop->threadpool;
    struct cpb_task task;
    task.err = cpb_make_error(CPB_OK);
    task.run = cpb_request_async_write_runner;
    task.msg.u.iip.arg1 = 0;
    task.msg.u.iip.arg2 = 0;
    task.msg.u.iip.argp = rqstate;
    rv = cpb_threadpool_push_task(tp, task);
    ret:

    return cpb_make_error(rv);

}


static struct cpb_error cpb_response_write(struct cpb_request_state *rqstate) {
    struct cpb_response_state *rsp = &rqstate->resp;
    int rv = CPB_OK;
    if (rsp->state != CPB_HTTP_R_ST_SENDING) {
        rv = CPB_INVALID_STATE_ERR;
        goto ret;
    }

    int total_bytes  = rsp->status_len + rsp->headers_len + rsp->body_len;

    int current_written_bytes = rsp->written_bytes;
    struct cpb_event ev;

    if (current_written_bytes < total_bytes) {
        size_t offset =  rsp->status_begin_index + current_written_bytes;

        ssize_t written = write(rqstate->socket_fd,
                                    rsp->output_buffer + offset,
                                    total_bytes - current_written_bytes);

        if (written == -1) {
            if (errno != EWOULDBLOCK && errno != EAGAIN) {
                rv =  CPB_WRITE_ERR;
                goto ret;
            }
        }
        else {
            current_written_bytes += written;
        }
    }
    struct cpb_error cerr = cpb_response_on_bytes_written(rqstate, rsp->written_bytes, current_written_bytes - rsp->written_bytes);
    
    ret:

    return cerr;
}

static int cpb_response_end_i(struct cpb_request_state *rqstate) {
    struct cpb_response_state *rsp = &rqstate->resp;
    struct cpb_http_multiplexer *m = cpb_server_get_multiplexer_i(rqstate->server, rqstate->socket_fd);
    int is_next = m->next_response == rqstate;

    if (rsp->state == CPB_HTTP_R_ST_SENDING ||
        rsp->state == CPB_HTTP_R_ST_DONE    ||
        rsp->state == CPB_HTTP_R_ST_DEAD      ) 
    {
        cpb_assert_h(rsp->state != CPB_HTTP_R_ST_DEAD, "");

        return CPB_INVALID_STATE_ERR;
    }
    
    int rv;
    
    struct cpb_str name;
    cpb_str_init_const_str(&name, "Content-Length");
    int body_len = rsp->body_len;
    struct cpb_str body_len_str;
    cpb_str_init(rqstate->server->cpb, &body_len_str);
    cpb_str_itoa(rqstate->server->cpb, &body_len_str, body_len);
    rv = cpb_response_set_header(rqstate, &name, &body_len_str); //owns body_len_str
    if (rv != CPB_OK) {
        cpb_str_deinit(rqstate->server->cpb, &body_len_str);

        return rv;
    }

    if (!rqstate->is_persistent) {
        struct cpb_str name, value;
        cpb_str_init_const_str(&name, "Connection");
        cpb_str_init_const_str(&value, "close");
        rv = cpb_response_add_header(rqstate, &name, &value);
    }
    else if (rqstate->http_minor == 0) {
        struct cpb_str name, value;
        cpb_str_init_const_str(&name, "Connection");
        cpb_str_init_const_str(&value, "keep-alive");
        rv = cpb_response_add_header(rqstate, &name, &value);
    }
    if (rv != CPB_OK) {

        return rv;
    }
        
    rv = cpb_response_prepare_headers(rqstate, rqstate->eloop);
    if (rv != CPB_OK){

        return rv;
    }
        

    rsp->state = CPB_HTTP_R_ST_SENDING;
    
    if (is_next) {
        //TODO: will it be an issue if this gets scheduled twice? 
        //  [Whatever event we are on] -> we schedule this here
        //  [select() event] -> schedules this too
        //  [First  time scheduled]
        //  [Second time scheduled]
        // ^ this was handled by adding is_read_scheduled and is_send_scheduled flags
        //   need to confirm this solves i
        RQSTATE_EVENT(stderr, "Scheduled rqstate %p for send because cpb_response_end() was called, socket %d\n", rqstate, m->socket_fd);
        rqstate->is_send_scheduled = 1;
        struct cpb_event ev;
        cpb_event_http_init(rqstate->server, &ev, CPB_HTTP_SEND, rqstate, 0);
        ev.handle(ev);
    }

    return rv;
}


/*This is not the only source of bytes the request has, see also cpb_request_fork*/
static struct cpb_error cpb_request_read_from_client(struct cpb_request_state *rqstate) {
    int socket = rqstate->socket_fd;
    int avbytes = cpb_request_input_buffer_size(rqstate) - rqstate->input_buffer_len - 1;
    int nbytes;
    struct cpb_error err = {0};
    nbytes = read(socket, rqstate->input_buffer + rqstate->input_buffer_len, avbytes);

    if (nbytes < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            err = cpb_make_error(CPB_OK);
            goto ret;
        }
        else {
            err = cpb_make_error(CPB_READ_ERR);
            //fprintf(stderr, "READ ERROR");
            goto ret;
        }
    }
    else if (nbytes == 0) {
        struct cpb_event ev;
        if (avbytes == 0) {
            cpb_event_http_init(rqstate->server, &ev, CPB_HTTP_INPUT_BUFFER_FULL, rqstate, 0);
        }
        else {
            cpb_event_http_init(rqstate->server, &ev, CPB_HTTP_CLIENT_CLOSED, rqstate, 0);
        }
        //TODO error handling, also why not directly deal with the event
        cpb_eloop_append(rqstate->eloop, ev); 
        //fprintf(stderr, "EOF");
    }
    else {
        avbytes -= nbytes;
        int idx = rqstate->input_buffer_len;
        err = cpb_request_on_bytes_read(rqstate, idx, nbytes);
        if (err.error_code != CPB_OK)
            goto ret;
    }
    ret:

    return err;
}

static void on_http_read_sync(struct cpb_event ev) {
    struct cpb_request_state *rqstate = ev.msg.u.iip.argp;
    struct cpb_error err = cpb_make_error(CPB_OK);
    cpb_assert_h(rqstate->is_read_scheduled, "");
    RQSTATE_EVENT(stderr, "Handling CPB_HTTP_READ for rqstate %p\n", rqstate);
    rqstate->is_read_scheduled = 0;
    err = cpb_request_read_from_client(rqstate);
    if (err.error_code != CPB_OK) {
        cpb_request_handle_http_error(rqstate);
    }
    return err;
}

static void on_http_read_async(struct cpb_event ev) {
    struct cpb_request_state *rqstate = ev.msg.u.iip.argp;
    struct cpb_error err = cpb_make_error(CPB_OK);
    cpb_assert_h(rqstate->is_read_scheduled, "");
    RQSTATE_EVENT(stderr, "Handling CPB_HTTP_READ for rqstate %p\n", rqstate);
    cpb_request_async_read_from_client(rqstate);
    return err;
}


static void on_http_send_sync(struct cpb_event ev) {
    struct cpb_request_state *rqstate = ev.msg.u.iip.argp;
    struct cpb_error err = cpb_make_error(CPB_OK);
    cpb_assert_h(rqstate->is_send_scheduled, "");
    cpb_assert_h(rqstate->resp.state == CPB_HTTP_R_ST_SENDING, "HTTP_SEND scheduled on an unready response");
    RQSTATE_EVENT(stderr, "Handling CPB_HTTP_SEND for rqstate %p\n", rqstate);    

    err = cpb_response_write(rqstate);
    rqstate->is_send_scheduled = 0;
    if (err.error_code != CPB_OK) {
        cpb_request_handle_socket_error(rqstate);
        goto ret;
    }
    if (rqstate->resp.state == CPB_HTTP_R_ST_DONE) {
        cpb_request_on_response_done(rqstate);
    }

    ret:
    return err;
}


static void on_http_send_async(struct cpb_event ev) {
    struct cpb_request_state *rqstate = ev.msg.u.iip.argp;
    struct cpb_error err = cpb_make_error(CPB_OK);
    cpb_assert_h(rqstate->is_send_scheduled, "");
    cpb_assert_h(rqstate->resp.state == CPB_HTTP_R_ST_DONE, "HTTP_SEND scheduled on an unready response");
    RQSTATE_EVENT(stderr, "Handling CPB_HTTP_SEND for rqstate %p\n", rqstate);
    
    cpb_assert_h(rqstate->resp.state != CPB_HTTP_R_ST_DEAD, "");

    cpb_response_async_write(rqstate);

    ret:
    return err;
}

static void on_http_input_buffer_full(struct cpb_event ev) {
    struct cpb_request_state *rqstate = ev.msg.u.iip.argp;
    struct cpb_error err = cpb_make_error(CPB_OK);
    RQSTATE_EVENT(stderr, "INPUT BUFFER FULL For rqstate %p, sz: %d\n", rqstate, rqstate->input_buffer_cap);
    int rv;
    if ((rv = cpb_request_input_buffer_ensure_cap(rqstate, rqstate->input_buffer_cap * 2)) != CPB_OK) {
        cpb_request_handle_socket_error(rqstate);
        err = cpb_make_error(rv);
    }
    cpb_assert_h(rqstate->is_read_scheduled, "");
    rqstate->is_read_scheduled = 0;
    /*TODO: else reschedule?*/
}
static void on_http_did_read(struct cpb_event ev) {
    struct cpb_request_state *rqstate = ev.msg.u.iip.argp;
    struct cpb_error err = cpb_make_error(CPB_OK);
    RQSTATE_EVENT(stderr, "Handling async read notification of rqstate %p, %d bytes\n", rqstate, ev.msg.u.iip.arg1);
    cpb_assert_h(rqstate->is_read_scheduled, "");
    rqstate->is_read_scheduled = 0;
    
    cpb_assert_h(rqstate->istate != CPB_HTTP_I_ST_DEAD, ""); 
    int len  = ev.msg.u.iip.arg1;
    err = cpb_request_on_bytes_read(rqstate, rqstate->input_buffer_len, len);
    return err;
}

static void on_http_did_write(struct cpb_event ev) {
    struct cpb_request_state *rqstate = ev.msg.u.iip.argp;
    struct cpb_error err = cpb_make_error(CPB_OK);
    RQSTATE_EVENT(stderr, "Handling async write notification of rqstate %p, %d bytes\n", rqstate, ev.msg.u.iip.arg1);
    cpb_assert_h(rqstate->is_send_scheduled, "");
    rqstate->is_send_scheduled = 0;
    cpb_assert_h(rqstate->resp.state != CPB_HTTP_R_ST_DEAD, "");
    int len  = ev.msg.u.iip.arg1;
    err = cpb_response_on_bytes_written(rqstate, rqstate->resp.written_bytes, len);
    if (rqstate->resp.state == CPB_HTTP_R_ST_DONE) {
        cpb_request_on_response_done(rqstate);
    }
}

static void on_http_client_closed(struct cpb_event ev) {
    struct cpb_request_state *rqstate = ev.msg.u.iip.argp;
    /*TODO: This was closed by the client, check if it's valid*/
    /*For example if we are waiting for a request body and connection was closed prematurely*/
    RQSTATE_EVENT(stderr, "Client closed for rqstate %p, socket %d\n", rqstate, rqstate->socket_fd);
    cpb_server_cancel_requests(rqstate->server, rqstate->socket_fd);
}

static void on_http_cancel(struct cpb_event ev) {
    //being called on the socket/multiplexer not on a request
    struct cpb_server *s = ev.msg.u.iip.argp;
    int socket_fd = ev.msg.u.iip.arg1;
    struct cpb_http_multiplexer *mp = cpb_server_get_multiplexer_i(s, socket_fd);
    RQSTATE_EVENT(stderr, "Handling Cancel event for socket %d\n", socket_fd);
    cpb_assert_h(mp->state == CPB_MP_CANCELLING, "invalid mp state");
    cpb_assert_h(mp->socket_fd == socket_fd, "invalid mp state");
    struct cpb_request_state *next = mp->next_response;
    if (next != mp->creading && mp->creading) {
        cpb_assert_h(mp->creading->is_cancelled, "");
        cpb_request_on_request_done(mp->creading);
    }
    mp->creading = NULL;
    while (next) {
        cpb_assert_h(next->is_cancelled, "");
        struct cpb_request_state *tmp = next;
        next = next->next_rqstate;
        cpb_request_on_request_done(tmp);
    }
    mp->next_response = NULL;
    cpb_server_close_connection(s, socket_fd);
}

static void on_http_read_io_error(struct cpb_event ev) {
    struct cpb_request_state *rqstate = ev.msg.u.iip.argp;
    cpb_assert_h(rqstate->is_read_scheduled, "");
    rqstate->is_read_scheduled = 0;
    cpb_request_handle_socket_error(rqstate);
}


static void on_http_write_io_error(struct cpb_event ev) {
    struct cpb_request_state *rqstate = ev.msg.u.iip.argp;
    cpb_assert_h(rqstate->is_send_scheduled, "");
    rqstate->is_send_scheduled = 0;
    cpb_request_handle_socket_error(rqstate);
}


static int cpb_event_http_init(struct cpb_server *s, struct cpb_event *ev, int cmd, void *object, int arg) {
    switch (cmd) {
        case CPB_HTTP_READ:               ev->handle = s->on_read;                 break;
        case CPB_HTTP_SEND:               ev->handle = s->on_send;                 break;
        case CPB_HTTP_INPUT_BUFFER_FULL:  ev->handle = on_http_input_buffer_full;  break;
        case CPB_HTTP_CLIENT_CLOSED:      ev->handle = on_http_client_closed;      break;
        case CPB_HTTP_READ_IO_ERROR:      ev->handle = on_http_read_io_error;      break;
        case CPB_HTTP_WRITE_IO_ERROR:     ev->handle = on_http_write_io_error;     break;
        case CPB_HTTP_DID_READ:           ev->handle = on_http_did_read;           break;
        case CPB_HTTP_DID_WRITE:          ev->handle = on_http_did_write;          break;
        case CPB_HTTP_CANCEL:             ev->handle = on_http_cancel;             break;
        default: cpb_assert_h(0, "invalid cmd"); break;
    }
    ev->msg.u.iip.arg1 = arg;
    ev->msg.u.iip.arg2 = cmd;
    ev->msg.u.iip.argp = object;
    return CPB_OK;
}

static void cpb_server_on_read_available_i(struct cpb_server *s, struct cpb_http_multiplexer *m) {
    struct cpb_event ev;
    cpb_assert_h((!!m) && m->state == CPB_MP_ACTIVE, "");
    cpb_assert_h(!!m->creading, "");
    cpb_assert_h(!m->creading->is_read_scheduled, "");
    
    int flags = s->config.http_use_aio;
    cpb_event_http_init(s, &ev, CPB_HTTP_READ, m->creading, flags);
    m->creading->is_read_scheduled = 1;
    cpb_eloop_append(m->eloop, ev);
    RQSTATE_EVENT(stderr, "Scheduled rqstate %p to be read, because we found out "
                    "read is available for socket %d\n", m->creading, m->socket_fd);
    
}
static void cpb_server_on_write_available_i(struct cpb_server *s, struct cpb_http_multiplexer *m) {
    struct cpb_event ev;
    int flags = s->config.http_use_aio;
    cpb_event_http_init(s, &ev, CPB_HTTP_SEND, m->next_response, flags);
    m->next_response->is_send_scheduled = 1;
    cpb_eloop_append(m->eloop, ev);
    
}

#endif //CPB_HTTP_SERVER_EVENTS_INTERNAL