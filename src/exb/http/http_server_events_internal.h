#ifndef EXB_HTTP_SERVER_EVENTS_INTERNAL
#define EXB_HTTP_SERVER_EVENTS_INTERNAL

//read/recv/write
#include <unistd.h>
#include <errno.h>

#include "http_server_events.h"
#include "http_server_internal.h"
#include "http_socket_multiplexer_internal.h"
#include "http_request.h"
#include "http_response.h"
#include "http_request_handler_resolution.h"
#include "../exb_build_config.h"
#include "http_parse.h"

static struct exb_error exb_request_on_bytes_read(struct exb_request_state *rqstate, int index, int nbytes);
static struct exb_error exb_response_on_bytes_written(struct exb_request_state *rqstate, int index, int nbytes);
enum exb_event_http_cmd {
    EXB_HTTP_READ, /*ask it to read, .argp : rqstate*/
    EXB_HTTP_SEND, /*ask it to write, .argp : rqstate*/
    EXB_HTTP_DID_READ,  /*inform about async read result, .argp : rqstate*/
    EXB_HTTP_DID_WRITE, /*inform about async write result, .argp : rqstate*/
    EXB_HTTP_WRITE_IO_ERROR, /*inform about error during async read/write, .argp : rqstate*/
    EXB_HTTP_READ_IO_ERROR,
    EXB_HTTP_INPUT_BUFFER_FULL, /*inform about a full buffer during async read, .argp : rqstate*/
    EXB_HTTP_CLIENT_CLOSED, //.argp : rqstate
    EXB_HTTP_CANCEL, /*.argp: http server, .arg1: socket*/
};

static int exb_event_http_init(struct exb_server *s, struct exb_http_multiplexer *mp, struct exb_event *ev, int cmd, void *object, int arg);

static void exb_request_on_request_done(struct exb_request_state *rqstate);

static void mark_read_scheduled(struct exb_request_state *rqstate, int mark) {
    if (mark)
        exb_assert_h(!rqstate->is_read_scheduled, "");
    else
        exb_assert_h(rqstate->is_read_scheduled, "");
    struct exb_http_multiplexer *mp = exb_server_get_multiplexer_i(rqstate->server, rqstate->socket_fd);
    rqstate->is_read_scheduled = !!mark;
    mp->wants_read = !mark;
}
static void mark_send_scheduled(struct exb_request_state *rqstate, int mark) {
    if (mark)
        exb_assert_h(!rqstate->is_send_scheduled, "");
    else
        exb_assert_h(rqstate->is_send_scheduled, "");
    struct exb_http_multiplexer *mp = exb_server_get_multiplexer_i(rqstate->server, rqstate->socket_fd);
    rqstate->is_send_scheduled = !!mark;
    mp->wants_write = !mark;
}

static void exb_request_handle_fatal_error(struct exb_request_state *rqstate) {
    //TODO should terminate connection not whole server
    abort();
}

static void exb_request_handle_socket_error(struct exb_request_state *rqstate) {
    if (rqstate->is_cancelled)
        return;
    exb_server_cancel_requests(rqstate->server, rqstate->socket_fd);
}

//TODO: refactor
static void exb_request_handle_http_error(struct exb_request_state *rqstate) {
    if (rqstate->is_cancelled)
        return;
    exb_server_cancel_requests(rqstate->server, rqstate->socket_fd);
}

/*TODO: Make sure this does the right thing if the handler already ended the request with an error*/
static void exb_http_request_on_handler_error(struct exb_request_state *rqstate) {
    exb_response_return_error(rqstate, 500, "internal error");
}

static void exb_request_call_handler(struct exb_request_state *rqstate, enum exb_request_handler_reason reason) {
    int rv = rqstate->request_handler(rqstate->rqh_state, rqstate, reason);
    if (rv != EXB_OK)  {
        exb_http_request_on_handler_error(rqstate);
    }
}


static void exb_request_resolve_and_call_handler(struct exb_request_state *rqstate) {
    exb_http_request_resolve(rqstate);
    int rv = rqstate->request_handler(rqstate->rqh_state, rqstate, EXB_HTTP_HANDLER_HEADERS);
    if (rv != EXB_OK)  {
        exb_http_request_on_handler_error(rqstate);
    }
}

static void exb_request_lifetime_checks(struct exb_http_multiplexer *mp, struct exb_request_state *rqstate) {
    if (!rqstate->is_read_scheduled && !rqstate->is_send_scheduled && mp->currently_reading != rqstate) {
        //FIXME: if cancelled, are we sure no one else has a reference to it?
        if ((rqstate->istate == EXB_HTTP_I_ST_DONE && rqstate->resp.state == EXB_HTTP_R_ST_DONE) || rqstate->is_cancelled) {
            if (!rqstate->is_cancelled)
                exb_assert_h(mp->currently_reading != rqstate && mp->next_response != rqstate, "");
            exb_server_destroy_rqstate(rqstate->server, mp->evloop, rqstate);
            RQSTATE_EVENT(stderr, "lifetime check for rqstate %p destroyed it\n", rqstate);
        }
    }
    else {
        RQSTATE_EVENT(stderr, "lifetime check for rqstate %p, "
                "didnt destroy it, because one of:"
                " state not done? %d read_scheduled? %d write_scheduled? %d\n", rqstate,
                                                                                rqstate->istate != EXB_HTTP_I_ST_DONE,
                                                                                rqstate->is_read_scheduled,
                                                                                rqstate->is_send_scheduled);
    }
}


static void exb_request_on_request_done(struct exb_request_state *rqstate) {
    exb_assert_h(!rqstate->is_read_scheduled, "");
    exb_assert_h(rqstate->istate == EXB_HTTP_I_ST_DONE || rqstate->is_cancelled, "");
    struct exb_http_multiplexer *mp = exb_server_get_multiplexer_i(rqstate->server, rqstate->socket_fd);
    exb_request_lifetime_checks(mp, rqstate);
}

static void exb_request_on_response_done(struct exb_request_state *rqstate) {
    exb_assert_h(!rqstate->is_send_scheduled, "");
    exb_assert_h(rqstate->resp.state == EXB_HTTP_R_ST_DONE, "");
    struct exb_http_multiplexer *mp = exb_server_get_multiplexer_i(rqstate->server, rqstate->socket_fd);
    exb_assert_h(mp->next_response == rqstate, "");
    exb_http_multiplexer_pop_response(mp);
    exb_request_lifetime_checks(mp, rqstate);
}


static struct exb_error exb_request_on_headers_read(struct exb_request_state *rqstate) {
    struct exb_error err  = exb_request_http_parse(rqstate);
    if (err.error_code != EXB_OK) {
        exb_request_handle_http_error(rqstate);
        /*TODO: IS IT ALWAYS A GOOD IDEA TO END THE CONNECTION HERE?*/
        return err;
        
    }
    int rv = exb_request_http_check_validity(rqstate);
    if (rv != EXB_OK) {
        exb_request_handle_http_error(rqstate);
        /*TODO: IS IT ALWAYS A GOOD IDEA TO END THE CONNECTION HERE?*/
        return exb_make_error(rv);
    }
    if (rqstate->is_chunked) {
        rqstate->parse_chunk_cursor = rqstate->body_s.index; //first chunk
    }

    return exb_make_error(EXB_OK);
}
static struct exb_error exb_request_fork(struct exb_request_state *rqstate) {

    struct exb_error err;
    struct exb_server *s = rqstate->server;
    struct exb_http_multiplexer *mp = exb_server_get_multiplexer_i(s, rqstate->socket_fd);
    if (mp->state != EXB_MP_ACTIVE)
        return exb_make_error(EXB_SOCKET_ERR);
    exb_assert_h(mp && mp->state == EXB_MP_ACTIVE, "");
    
    struct exb_request_state *forked_rqstate = exb_server_new_rqstate(s, mp->evloop, rqstate->socket_fd);
    //exb_assert_h(mp->currently_reading == rqstate, "Tried to fork a request that is not the current one reading on socket");

    //Copy mistakenly read bytes from older request to the new one

    int bytes_copied = rqstate->input_buffer_len - rqstate->next_request_cursor;

    RQSTATE_EVENT(stderr, "Forking %p to %p with %d leftover bytes\n", rqstate, forked_rqstate, bytes_copied);

    int rv;
    if ((rv = exb_request_input_buffer_ensure_cap(forked_rqstate, bytes_copied + 1)) != EXB_OK) {
        return exb_make_error(rv);
    }
    
    exb_assert_h(bytes_copied < exb_request_input_buffer_size(forked_rqstate), "");
    memcpy(forked_rqstate->input_buffer, rqstate->input_buffer + rqstate->next_request_cursor, bytes_copied);
    err = exb_request_on_bytes_read(forked_rqstate, 0, bytes_copied);
    if (err.error_code != EXB_OK) {
        exb_server_destroy_rqstate(s, mp->evloop, forked_rqstate);
        exb_server_cancel_requests(s, rqstate->socket_fd);
        goto ret;
    }
    
    exb_assert_h(!rqstate->is_forked, "");
    rqstate->is_forked = 1;
    exb_assert_h(!rqstate->is_read_scheduled, "");
    mp->currently_reading = forked_rqstate;
    exb_http_multiplexer_queue_response(mp, forked_rqstate);
    //RQSTATE_EVENT(stderr, "Scheduled rqstate %p to be read socket %d, because we just forked it\n",  forked_rqstate, forked_rqstate->socket_fd)
    err = exb_make_error(EXB_OK);
ret:

    return err;
}
static struct exb_error exb_request_on_bytes_read(struct exb_request_state *rqstate, int index, int nbytes) {
    struct exb_error err = {0};

    exb_assert_h(index == rqstate->input_buffer_len, "");
    rqstate->input_buffer_len += nbytes;
    rqstate->bytes_read += nbytes;
    int scan_idx = index - 3;
    scan_idx = scan_idx < 0 ? 0 : scan_idx;
    int scan_len = index + nbytes;
    
    //fprintf(stderr, "READ %d BYTES, TOTAL %d BYTES\n", nbytes, rqstate->input_buffer_len);

    if (rqstate->istate == EXB_HTTP_I_ST_DONE || rqstate->istate == EXB_HTTP_I_ST_DEAD) {
        err = exb_make_error(EXB_INVALID_STATE_ERR);
        goto ret;
    }

    if (rqstate->istate == EXB_HTTP_I_ST_INIT)
        rqstate->istate = EXB_HTTP_I_ST_WAITING_FOR_HEADERS;
    
    if (rqstate->istate == EXB_HTTP_I_ST_WAITING_FOR_HEADERS )
    {
        if (exb_str_has_crlfcrlf(rqstate->input_buffer, scan_idx, scan_len)) {
            err = exb_request_on_headers_read(rqstate);
            if (err.error_code != EXB_OK) {
                goto ret;
            }
            rqstate->istate = EXB_HTTP_I_ST_WAITING_FOR_BODY;
            exb_request_resolve_and_call_handler(rqstate);
        }
    }
    if (rqstate->istate == EXB_HTTP_I_ST_WAITING_FOR_BODY ) 
    {
        exb_assert_h(rqstate->pstate == EXB_HTTP_P_ST_DONE || rqstate->pstate == EXB_HTTP_P_ST_IN_CHUNKED_BODY, "");
        if (!exb_request_has_body(rqstate)) {
            rqstate->istate = EXB_HTTP_I_ST_DONE;
            RQSTATE_EVENT(stderr, "marked request %p as done, because it has no body\n", rqstate);
        }
        else if (rqstate->is_chunked && (exb_str_has_crlfcrlf(rqstate->input_buffer, scan_idx, scan_len))) {
            int rv = exb_request_http_parse_chunked_encoding(rqstate);
            if (rv != EXB_OK) {
                err = exb_make_error(rv);
                goto ret;
            }
            if (exb_request_is_chunked_body_complete(rqstate)) {
                rqstate->istate = EXB_HTTP_I_ST_DONE;
                RQSTATE_EVENT(stderr, "marked request %p as done, because its chunked body completed\n", rqstate);
            }
        }
        else if (exb_request_body_bytes_read(rqstate) >= rqstate->content_length) {
            if (rqstate->body_handling == EXB_HTTP_B_BUFFER) {
                rqstate->istate = EXB_HTTP_I_ST_DONE;
                RQSTATE_EVENT(stderr, "marked request %p as done, because its body completed\n", rqstate);
                rqstate->next_request_cursor = rqstate->body_s.index + rqstate->content_length;
                exb_assert_h(exb_str_is_const(&rqstate->body_decoded), "");
                rqstate->body_decoded = exb_str_slice_to_const_str(rqstate->body_s, rqstate->input_buffer);
                #if 0
                int rv = exb_str_strlappend(rqstate->server->exb, &rqstate->body_decoded, rqstate->input_buffer + rqstate->body_s.index, rqstate->content_length);
                if (rv != EXB_OK) {
                    err = exb_make_error(rv);
                    goto ret;
                }
                #endif
            }
            else {
                exb_assert_h(rqstate->body_handling == EXB_HTTP_B_DISCARD, "");
            }
            rqstate->istate = EXB_HTTP_I_ST_DONE;
        }
    }
    
    if (rqstate->istate == EXB_HTTP_I_ST_DONE) {

        exb_assert_h(rqstate->pstate == EXB_HTTP_P_ST_DONE, "");

        if (exb_request_has_body(rqstate)) {
            if (rqstate->body_handling == EXB_HTTP_B_BUFFER) {
                exb_request_call_handler(rqstate, EXB_HTTP_HANDLER_BODY);
            }
            else if (rqstate->body_handling != EXB_HTTP_B_DISCARD) {
                exb_assert_h(0, "");
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
            err = exb_request_fork(rqstate);
            if (err.error_code != EXB_OK) {
                exb_request_handle_socket_error(rqstate);
                goto ret;
            }
        }
        else {
            struct exb_http_multiplexer *mp = exb_server_get_multiplexer_i(rqstate->server, rqstate->socket_fd);
            mp->currently_reading = NULL;
            mp->wants_read = 0;
        }

        exb_request_on_request_done(rqstate);
    }
   
ret:

    return err;
}
//assumes rqstate->input_buffer_len WAS NOT adjusted for new bytes

/*assumes rsp->written_bytes WAS NOT adjusted for new bytes*/

static struct exb_error exb_response_on_bytes_written(struct exb_request_state *rqstate, int index, int nbytes) {
    struct exb_response_state *rsp = exb_request_get_response(rqstate);

    exb_assert_h(index == rsp->written_bytes, "");
    rsp->written_bytes += nbytes;
    int total_bytes  = rsp->status_len + rsp->headers_len + rsp->body_len;
    if (rsp->written_bytes >= total_bytes) {
        exb_assert_h(rsp->written_bytes == total_bytes, "wrote more than expected");
        rsp->state = EXB_HTTP_R_ST_DONE;
    }

    return exb_make_error(EXB_OK);
}

static void exb_request_async_read_from_client_runner(struct exb_thread *thread, struct exb_task *task);


static void exb_request_async_write_runner(struct exb_thread *thread, struct exb_task *task) {

    struct exb_request_state *rqstate = task->msg.u.iip.argp;
    struct exb_response_state *rsp = &rqstate->resp;
    struct exb_http_multiplexer *mp = exb_server_get_multiplexer(rqstate->server, rqstate->socket_fd);
    int rv = EXB_OK;
    int details = 0;
    int total_bytes  = rsp->status_len + rsp->headers_len + rsp->body_len;

    int current_written_bytes = rsp->written_bytes;
    

    if (current_written_bytes < total_bytes) {
        size_t offset =  rqstate->resp.status_begin_index + current_written_bytes;
        int sum = 0;
        dp_useless(sum);

#ifdef EXB_USE_READ_WRITE_FOR_TCP
        ssize_t written = write(rqstate->socket_fd,
                                rqstate->resp.output_buffer + offset,
                                total_bytes - current_written_bytes);
#else
        ssize_t written = send(rqstate->socket_fd,
                                            rsp->output_buffer + offset,
                                            total_bytes - current_written_bytes,
                                            MSG_DONTWAIT);
#endif
    
        if (written == -1) {
            if (errno != EWOULDBLOCK && errno != EAGAIN) {
                rv =  EXB_WRITE_ERR;
                details = errno;
                goto ret;
            }
        }
        else {
            //TODO: handle 0 case
            current_written_bytes += written;
        }
    }
ret:
    {
        struct exb_event ev;
        if (rv != EXB_OK) {
            exb_event_http_init(rqstate->server, mp, &ev, EXB_HTTP_WRITE_IO_ERROR, rqstate, current_written_bytes - rsp->written_bytes);
        }
        else {
            exb_event_http_init(rqstate->server, mp, &ev, EXB_HTTP_DID_WRITE, rqstate, current_written_bytes - rsp->written_bytes);
        }
        int err = exb_evloop_ts_append(rqstate->evloop, ev);
        if (err != EXB_OK) {
            /*we cannot afford to have this fail*/
            exb_request_handle_fatal_error(rqstate);
        }
    }

    return;
}
static void exb_request_async_read_from_client_runner(struct exb_thread *thread, struct exb_task *task);
static struct exb_error exb_request_async_read_from_client(struct exb_request_state *rqstate) {

    struct exb_threadpool *tp = rqstate->evloop->threadpool;
    struct exb_task task;
    task.err = exb_make_error(EXB_OK);
    task.run = exb_request_async_read_from_client_runner;
    task.msg.u.iip.arg1 = 0;
    task.msg.u.iip.arg2 = 0;
    task.msg.u.iip.argp = rqstate;
    int rv = exb_threadpool_push_task(tp, task);

    return exb_make_error(rv);
}



static int exb_response_end_i(struct exb_request_state *rqstate) {
    struct exb_response_state  *rsp = &rqstate->resp;
    struct exb_http_multiplexer *mp = exb_server_get_multiplexer_i(rqstate->server, rqstate->socket_fd);
    int is_next = mp->next_response == rqstate;

    if (rsp->state == EXB_HTTP_R_ST_SENDING ||
        rsp->state == EXB_HTTP_R_ST_DONE    ||
        rsp->state == EXB_HTTP_R_ST_DEAD      ) 
    {
        exb_assert_h(rsp->state != EXB_HTTP_R_ST_DEAD, "");
        return EXB_INVALID_STATE_ERR;
    }
    
    int rv;
    
    struct exb_str name;
    exb_str_init_const_str(&name, "Content-Length");
    int body_len = rsp->body_len;
    struct exb_str body_len_str;
    char body_len_buff[EXB_INT_DIGITS];
    exb_str_init_empty_by_local_buffer(&body_len_str, body_len_buff, EXB_INT_DIGITS);
    exb_str_itoa(rqstate->server->exb, &body_len_str, body_len);
    rv = exb_response_set_header(rqstate, &name, &body_len_str); //owns body_len_str
    exb_str_deinit(rqstate->server->exb, &body_len_str);
    if (rv != EXB_OK) {
        return rv;
    }

    if (!rqstate->is_persistent) {
        struct exb_str name, value;
        exb_str_init_const_str(&name, "Connection");
        exb_str_init_const_str(&value, "close");
        rv = exb_response_add_header(rqstate, &name, &value);
    }
    else if (rqstate->http_minor == 0) {
        struct exb_str name, value;
        exb_str_init_const_str(&name, "Connection");
        exb_str_init_const_str(&value, "keep-alive");
        rv = exb_response_add_header(rqstate, &name, &value);
    }
    if (rv != EXB_OK) {
        return rv;
    }
#ifdef EXB_HTTP_ADD_DATE_HEADER
    {
        struct exb_str name, value;
        exb_str_init_const_str(&name, "Date");
        exb_str_init_const_str(&value, "Fri, 29 May 2020 03:38:54 GMT");
        rv = exb_response_add_header(rqstate, &name, &value);
        if (rv != EXB_OK) {
            return rv;
        }
    }
#endif
    if (!rqstate->is_persistent) {
        struct exb_str name, value;
        exb_str_init_const_str(&name, "Connection");
        exb_str_init_const_str(&value, "close");
        rv = exb_response_add_header(rqstate, &name, &value);
    }
        
    rv = exb_response_prepare_headers(rqstate, rqstate->evloop);
    if (rv != EXB_OK){

        return rv;
    }
        

    rsp->state = EXB_HTTP_R_ST_SENDING;
    
    if (is_next) {
        //TODO: will it be an issue if this gets scheduled twice? 
        //  [Whatever event we are on] -> we schedule this here
        //  [select() event] -> schedules this too
        //  [First  time scheduled]
        //  [Second time scheduled]
        // ^ this was handled by adding is_read_scheduled and is_send_scheduled flags
        //   need to confirm this solves i
        RQSTATE_EVENT(stderr, "Scheduled rqstate %p for send because exb_response_end() was called, socket %d\n", rqstate, mp->socket_fd);
        mark_send_scheduled(rqstate, 1);
        struct exb_event ev;
        exb_event_http_init(rqstate->server, mp, &ev, EXB_HTTP_SEND, rqstate, 0);
        ev.handle(ev);
    }

    return rv;
}

/*each of these IO functions should do these tasks:
    for read functions:
        read from socket to request's input buffer
        mark rqstate as not read scheduled
        call exb_request_on_bytes_read
    for send functions:
        write to socket from response's output buffer
        call on_bytes_written
        mark rqstate as not send scheduled
        if state is EXB_HTTP_R_ST_DONE then call on_request_done


*/
static void on_http_send_sync(struct exb_event ev);
static void on_http_read_sync(struct exb_event ev);
static void on_http_send_async(struct exb_event ev);
static void on_http_read_async(struct exb_event ev);

static void on_http_ssl_send_sync(struct exb_event ev);
static void on_http_ssl_read_sync(struct exb_event ev);
static void on_http_ssl_send_async(struct exb_event ev);
static void on_http_ssl_read_async(struct exb_event ev);

/*This is not the only source of bytes the request has, see also exb_request_fork*/
static struct exb_error exb_request_read_from_client(struct exb_request_state *rqstate) {
    int socket = rqstate->socket_fd;
    struct exb_http_multiplexer *mp = exb_server_get_multiplexer(rqstate->server, rqstate->socket_fd);
    int avbytes = exb_request_input_buffer_size(rqstate) - rqstate->input_buffer_len - 1;
    int nbytes;
    struct exb_error err = {0};
    
#ifdef EXB_USE_READ_WRITE_FOR_TCP
    nbytes = read(socket, rqstate->input_buffer + rqstate->input_buffer_len, avbytes);
#else
    nbytes = recv(socket, rqstate->input_buffer + rqstate->input_buffer_len, avbytes, MSG_DONTWAIT);
#endif

    if (nbytes < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            err = exb_make_error(EXB_OK);
            goto ret;
        }
        else {
            err = exb_make_error(EXB_READ_ERR);
            //fprintf(stderr, "READ ERROR");
            goto ret;
        }
    }
    else if (nbytes == 0) {
        struct exb_event ev;
        if (avbytes == 0) {
            exb_event_http_init(rqstate->server, mp, &ev, EXB_HTTP_INPUT_BUFFER_FULL, rqstate, 0);
        }
        else {
            exb_event_http_init(rqstate->server, mp, &ev, EXB_HTTP_CLIENT_CLOSED, rqstate, 0);
        }
        //TODO error handling, also why not directly deal with the event
        exb_evloop_append(rqstate->evloop, ev); 
    }
    else {
        avbytes -= nbytes;
        int idx = rqstate->input_buffer_len;
        err = exb_request_on_bytes_read(rqstate, idx, nbytes);
        if (err.error_code != EXB_OK)
            goto ret;
    }
    ret:

    return err;
}

static void exb_request_async_read_from_client_runner(struct exb_thread *thread, struct exb_task *task) {
    struct exb_request_state *rqstate = task->msg.u.iip.argp;
    struct exb_http_multiplexer *mp = exb_server_get_multiplexer(rqstate->server, rqstate->socket_fd);
    int socket = rqstate->socket_fd;
    int avbytes = exb_request_input_buffer_size(rqstate) - rqstate->input_buffer_len - 1;
    int nbytes;
    
    int err = EXB_OK;
    int client_closed = 0;

    int buffer_current_bytes = rqstate->bytes_read;

    struct exb_event ev;
    int read_bytes;

#ifdef EXB_USE_READ_WRITE_FOR_TCP
    nbytes = read(socket, rqstate->input_buffer + rqstate->input_buffer_len, avbytes);
#else
    nbytes = recv(socket, rqstate->input_buffer + rqstate->input_buffer_len, avbytes, MSG_DONTWAIT);
#endif

    if (nbytes < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            err = EXB_OK;
            RQSTATE_EVENT(stderr, "async read to rqstate %p, would block\n", rqstate, rqstate->socket_fd);
        }
        else {
            err = EXB_READ_ERR;
        }
    }
    else if (nbytes == 0) {
        if (avbytes == 0) {
            err = EXB_BUFFER_FULL_ERR;
        }
        else {
            client_closed = 1;
        }
    }
    else {
        avbytes -= nbytes;
        buffer_current_bytes += nbytes;
    }

    /*TODO: handle error*/
    /*TODO: provide more details than what can fit in an event*/
    
    read_bytes = buffer_current_bytes - rqstate->bytes_read;

    if (err != EXB_OK) {
        if (err == EXB_BUFFER_FULL_ERR) {
            //we cannot do reallocation here, evloop_* functions are not thread safe
            exb_event_http_init(rqstate->server, mp, &ev, EXB_HTTP_INPUT_BUFFER_FULL, rqstate, read_bytes);
            RQSTATE_EVENT(stderr, "async read to rqstate %p, socket %d, with %d bytes, added buffer full event\n", rqstate, rqstate->socket_fd, read_bytes);
        }
        else {
            exb_event_http_init(rqstate->server, mp, &ev, EXB_HTTP_READ_IO_ERROR, rqstate, read_bytes);
            RQSTATE_EVENT(stderr, "async read to rqstate %p, socket %d, with %d bytes, added io error event\n", rqstate, rqstate->socket_fd, read_bytes);
        }
        
        int perr = exb_evloop_ts_append(rqstate->evloop, ev);
        if (perr != EXB_OK) {
            /*we cannot afford to have this fail*/
            exb_request_handle_fatal_error(rqstate);
        }
    }
    else {
        //even if it's zero we need to send this to mark it as not scheduled
        exb_event_http_init(rqstate->server, mp, &ev, EXB_HTTP_DID_READ, rqstate, read_bytes);
        int perr = exb_evloop_ts_append(rqstate->evloop, ev);
        if (perr != EXB_OK) {
            /*we cannot afford to have this fail*/
            exb_request_handle_fatal_error(rqstate);
        }
        RQSTATE_EVENT(stderr, "async read to rqstate %p, socket %d, with %d bytes, added read event\n", rqstate, rqstate->socket_fd, read_bytes);
        
        if (client_closed) {
            exb_event_http_init(rqstate->server, mp, &ev, EXB_HTTP_CLIENT_CLOSED, rqstate, read_bytes);
            perr = exb_evloop_ts_append(rqstate->evloop, ev);
            if (err != EXB_OK) {
                /*we cannot afford to have this fail*/
                exb_request_handle_fatal_error(rqstate);
            }
            RQSTATE_EVENT(stderr, "async read to rqstate %p, socket %d, with %d bytes client closed\n", rqstate, rqstate->socket_fd, read_bytes);
        }
    }
    RQSTATE_EVENT(stderr, "Did async read to rqstate %p, socket %d, with %d bytes\n", rqstate, rqstate->socket_fd, read_bytes);
ret:    

    return;
}


static void on_http_read_sync(struct exb_event ev) {
    struct exb_request_state *rqstate = ev.msg.u.iip.argp;
    struct exb_error err = exb_make_error(EXB_OK);
    mark_read_scheduled(rqstate, 0);
    RQSTATE_EVENT(stderr, "Handling EXB_HTTP_READ for rqstate %p\n", rqstate);
    err = exb_request_read_from_client(rqstate);
    if (err.error_code != EXB_OK) {
        exb_request_handle_http_error(rqstate);
    }
    
}

static void on_http_read_async(struct exb_event ev) {
    struct exb_request_state *rqstate = ev.msg.u.iip.argp;
    struct exb_error err = exb_make_error(EXB_OK);
    exb_assert_h(rqstate->is_read_scheduled, "");
    RQSTATE_EVENT(stderr, "Handling EXB_HTTP_READ for rqstate %p\n", rqstate);
    exb_request_async_read_from_client(rqstate);
    
}


static void on_http_send_sync(struct exb_event ev) {
    struct exb_request_state *rqstate = ev.msg.u.iip.argp;
    struct exb_response_state *rsp = &rqstate->resp;
    struct exb_error err = exb_make_error(EXB_OK);
    exb_assert_h(rqstate->is_send_scheduled, "");
    exb_assert_h(rqstate->resp.state == EXB_HTTP_R_ST_SENDING, "HTTP_SEND scheduled on an unready response");
    RQSTATE_EVENT(stderr, "Handling EXB_HTTP_SEND for rqstate %p\n", rqstate);    

    int total_bytes  = rsp->status_len + rsp->headers_len + rsp->body_len;
    int current_written_bytes = rsp->written_bytes;

    if (current_written_bytes < total_bytes) {
        size_t offset =  rsp->status_begin_index + current_written_bytes;
#ifdef EXB_USE_READ_WRITE_FOR_TCP
        ssize_t written = write(rqstate->socket_fd,
                                    rsp->output_buffer + offset,
                                    total_bytes - current_written_bytes);
#else
        ssize_t written = send(rqstate->socket_fd,
                                    rsp->output_buffer + offset,
                                    total_bytes - current_written_bytes,
                                    MSG_DONTWAIT);
#endif
        if (written == -1) {
            if (errno != EWOULDBLOCK && errno != EAGAIN) {
                err =  exb_make_error(EXB_WRITE_ERR);
                return;
            }
        }
        else {
            //TODO: handle 0 case
            current_written_bytes += written;
        }
    }
    err = exb_response_on_bytes_written(rqstate, rsp->written_bytes, current_written_bytes - rsp->written_bytes);
    
    struct exb_http_multiplexer *mp = exb_server_get_multiplexer_i(rqstate->server, rqstate->socket_fd);
    exb_assert_h(mp->next_response == rqstate, "");
    exb_assert_h(!mp->wants_write, "");
    if (err.error_code != EXB_OK) {
        exb_request_handle_socket_error(rqstate);
        return;
    }
    mark_send_scheduled(rqstate, 0);
    if (rqstate->resp.state == EXB_HTTP_R_ST_DONE) {
        exb_request_on_response_done(rqstate);
    }
}


static void on_http_send_async(struct exb_event ev) {
    struct exb_request_state *rqstate = ev.msg.u.iip.argp;
    struct exb_error err = exb_make_error(EXB_OK);
    exb_assert_h(rqstate->is_send_scheduled, "");
    exb_assert_h(rqstate->resp.state == EXB_HTTP_R_ST_DONE, "HTTP_SEND scheduled on an unready response");
    RQSTATE_EVENT(stderr, "Handling EXB_HTTP_SEND for rqstate %p\n", rqstate);
    
    exb_assert_h(rqstate->resp.state != EXB_HTTP_R_ST_DEAD, "");

    struct exb_response_state *rsp = &rqstate->resp;
    int rv = EXB_OK;
    if (rsp->state != EXB_HTTP_R_ST_SENDING) {
        rv = EXB_INVALID_STATE_ERR;
        goto ret;
    }
    struct exb_threadpool *tp = rqstate->evloop->threadpool;
    struct exb_task task;
    task.err = exb_make_error(EXB_OK);
    task.run = exb_request_async_write_runner;
    task.msg.u.iip.arg1 = 0;
    task.msg.u.iip.arg2 = 0;
    task.msg.u.iip.argp = rqstate;
    rv = exb_threadpool_push_task(tp, task);
    ret:

    return;
}



static void on_http_ssl_send_sync(struct exb_event ev) {
    struct exb_request_state *rqstate = ev.msg.u.iip.argp;
    struct exb_response_state *rsp = &rqstate->resp;
    struct exb_http_multiplexer *mp = exb_server_get_multiplexer_i(rqstate->server, rqstate->socket_fd);
    
    exb_assert_h(rqstate->is_send_scheduled, "");
    exb_assert_h(rqstate->resp.state == EXB_HTTP_R_ST_SENDING, "HTTP_SEND scheduled on an unready response");
    RQSTATE_EVENT(stderr, "Handling EXB_HTTP_SEND [SSL] for rqstate %p\n", rqstate);    

    
    int total_bytes  = rsp->status_len + rsp->headers_len + rsp->body_len;
    int current_written_bytes = rsp->written_bytes;

    struct exb_ssl_interface *ssl_if = &rqstate->server->ssl_interface;
    
    if (current_written_bytes < total_bytes) {
        size_t offset =  rsp->status_begin_index + current_written_bytes;
        struct exb_io_result sres;
        sres = ssl_if->ssl_connection_write(ssl_if->module,
                                            mp,
                                            rsp->output_buffer + offset,
                                            total_bytes - current_written_bytes);
        if (sres.flags & EXB_IO_FLAG_CLIENT_CLOSED) {
            struct exb_event nev;
            exb_event_http_init(rqstate->server, mp, &nev, EXB_HTTP_CLIENT_CLOSED, rqstate, 0);
            //TODO error handling, also why not directly deal with the event
            exb_evloop_append(rqstate->evloop, nev);
            return;
        }
        else if (sres.flags) {
            exb_request_handle_http_error(rqstate);
            return;
        }
        
        current_written_bytes += sres.nbytes;
    }
    exb_assert_h(mp->next_response == rqstate, "");
    mark_send_scheduled(rqstate, 0);
    struct exb_error err = exb_response_on_bytes_written(rqstate, rsp->written_bytes, current_written_bytes - rsp->written_bytes);
    
    
    if (err.error_code != EXB_OK) {
        exb_request_handle_socket_error(rqstate);
        return;
    }
    if (rqstate->resp.state == EXB_HTTP_R_ST_DONE) {
        exb_request_on_response_done(rqstate);
    }
}
static void on_http_ssl_read_sync(struct exb_event ev) {
    struct exb_request_state *rqstate = ev.msg.u.iip.argp;
    struct exb_http_multiplexer *mp = exb_server_get_multiplexer_i(rqstate->server, rqstate->socket_fd);

    mark_read_scheduled(rqstate, 0);
    RQSTATE_EVENT(stderr, "Handling EXB_HTTP_READ [SSL] for rqstate %p\n", rqstate);
    int avbytes = exb_request_input_buffer_size(rqstate) - rqstate->input_buffer_len - 1;

    if (avbytes == 0) {
        struct exb_event nev;
        exb_event_http_init(rqstate->server, mp, &nev, EXB_HTTP_INPUT_BUFFER_FULL, rqstate, 0);
        exb_evloop_append(rqstate->evloop, nev);
        return;
    }

    struct exb_ssl_interface *ssl_if = &rqstate->server->ssl_interface;
    
    struct exb_io_result rres;
    
    rres = ssl_if->ssl_connection_read(ssl_if->module,
                                       mp,
                                       rqstate->input_buffer + rqstate->input_buffer_len,
                                       avbytes);
    if (rres.flags & EXB_IO_FLAG_CLIENT_CLOSED) {
        struct exb_event nev;
        exb_event_http_init(rqstate->server, mp, &nev, EXB_HTTP_CLIENT_CLOSED, rqstate, 0);
        //TODO error handling, also why not directly deal with the event
        exb_evloop_append(rqstate->evloop, nev);
        return;
    }
    else if (rres.flags) {
        exb_request_handle_http_error(rqstate);
        return;
    }
    
    avbytes -= rres.nbytes;
    int idx = rqstate->input_buffer_len;
    struct exb_error err = exb_request_on_bytes_read(rqstate, idx, rres.nbytes);
    if (err.error_code != EXB_OK) {
        exb_request_handle_http_error(rqstate);
    }
}
static void on_http_ssl_send_async(struct exb_event ev) {
    //TODO:
    struct exb_request_state *rqstate = ev.msg.u.iip.argp;
    exb_log_error(rqstate->server->exb, "on_http_ssl_send_async not supported");
    abort();
}
static void on_http_ssl_read_async(struct exb_event ev) {
    //TODO:
    struct exb_request_state *rqstate = ev.msg.u.iip.argp;
    exb_log_error(rqstate->server->exb, "on_http_ssl_read_async not supported");
    abort();
}



static void on_http_input_buffer_full(struct exb_event ev) {
    struct exb_request_state *rqstate = ev.msg.u.iip.argp;
    struct exb_error err = exb_make_error(EXB_OK);
    RQSTATE_EVENT(stderr, "INPUT BUFFER FULL For rqstate %p, sz: %d\n", rqstate, rqstate->input_buffer_cap);
    int rv;
    if ((rv = exb_request_input_buffer_ensure_cap(rqstate, rqstate->input_buffer_cap * 2)) != EXB_OK) {
        exb_request_handle_socket_error(rqstate);
        err = exb_make_error(rv);
    }
    mark_read_scheduled(rqstate, 0);
    /*TODO: else reschedule?*/
}

static void on_http_did_read(struct exb_event ev) {
    struct exb_request_state *rqstate = ev.msg.u.iip.argp;
    struct exb_error err = exb_make_error(EXB_OK);
    RQSTATE_EVENT(stderr, "Handling async read notification of rqstate %p, %d bytes\n", rqstate, ev.msg.u.iip.arg1);
    mark_read_scheduled(rqstate, 0);
    
    
    exb_assert_h(rqstate->istate != EXB_HTTP_I_ST_DEAD, ""); 
    int len  = ev.msg.u.iip.arg1;
    err = exb_request_on_bytes_read(rqstate, rqstate->input_buffer_len, len);
}

static void on_http_did_write(struct exb_event ev) {
    struct exb_request_state *rqstate = ev.msg.u.iip.argp;
    struct exb_error err = exb_make_error(EXB_OK);
    RQSTATE_EVENT(stderr, "Handling async write notification of rqstate %p, %d bytes\n", rqstate, ev.msg.u.iip.arg1);
    mark_send_scheduled(rqstate, 0);
    exb_assert_h(rqstate->resp.state != EXB_HTTP_R_ST_DEAD, "");
    int len  = ev.msg.u.iip.arg1;
    err = exb_response_on_bytes_written(rqstate, rqstate->resp.written_bytes, len);
    if (rqstate->resp.state == EXB_HTTP_R_ST_DONE) {
        exb_request_on_response_done(rqstate);
    }
}

static void on_http_client_closed(struct exb_event ev) {
    struct exb_request_state *rqstate = ev.msg.u.iip.argp;
    /*TODO: This was closed by the client, check if it's valid*/
    /*For example if we are waiting for a request body and connection was closed prematurely*/
    RQSTATE_EVENT(stderr, "Client closed for rqstate %p, socket %d\n", rqstate, rqstate->socket_fd);
    exb_server_cancel_requests(rqstate->server, rqstate->socket_fd);
}

static void on_http_cancel(struct exb_event ev) {
    //being called on the socket/multiplexer not on a request
    struct exb_server *s = ev.msg.u.iip.argp;
    int socket_fd = ev.msg.u.iip.arg1;
    struct exb_http_multiplexer *mp = exb_server_get_multiplexer_i(s, socket_fd);
    RQSTATE_EVENT(stderr, "Handling Cancel event for socket %d\n", socket_fd);
    exb_assert_h(mp->state == EXB_MP_CANCELLING, "invalid mp state");
    exb_assert_h(mp->socket_fd == socket_fd, "invalid mp state");
    struct exb_request_state *next = mp->next_response;
    if (next != mp->currently_reading && mp->currently_reading) {
        exb_assert_h(mp->currently_reading->is_cancelled, "");
        exb_request_on_request_done(mp->currently_reading);
    }
    mp->currently_reading = NULL;
    while (next) {
        exb_assert_h(next->is_cancelled, "");
        struct exb_request_state *tmp = next;
        next = next->next_rqstate;
        exb_request_on_request_done(tmp);
    }
    mp->next_response = NULL;
    mp->wants_read  = 0;
    mp->wants_write = 0;
    
}

static void on_http_read_io_error(struct exb_event ev) {
    struct exb_request_state *rqstate = ev.msg.u.iip.argp;
    exb_assert_h(rqstate->is_read_scheduled, "");
    
    struct exb_http_multiplexer *mp = exb_server_get_multiplexer_i(rqstate->server, rqstate->socket_fd);
    mark_read_scheduled(rqstate, 0);
    exb_request_handle_socket_error(rqstate);
}


static void on_http_write_io_error(struct exb_event ev) {
    struct exb_request_state *rqstate = ev.msg.u.iip.argp;
    struct exb_http_multiplexer *mp = exb_server_get_multiplexer_i(rqstate->server, rqstate->socket_fd);
    mark_send_scheduled(rqstate, 0);
    exb_request_handle_socket_error(rqstate);
}


static inline int exb_event_http_init(struct exb_server *s, struct exb_http_multiplexer *mp, struct exb_event *ev, int cmd, void *object, int arg) {
    switch (cmd) {
        case EXB_HTTP_READ:               ev->handle = mp->on_read;                 break;
        case EXB_HTTP_SEND:               ev->handle = mp->on_send;                 break;
        case EXB_HTTP_INPUT_BUFFER_FULL:  ev->handle = on_http_input_buffer_full;  break;
        case EXB_HTTP_CLIENT_CLOSED:      ev->handle = on_http_client_closed;      break;
        case EXB_HTTP_READ_IO_ERROR:      ev->handle = on_http_read_io_error;      break;
        case EXB_HTTP_WRITE_IO_ERROR:     ev->handle = on_http_write_io_error;     break;
        case EXB_HTTP_DID_READ:           ev->handle = on_http_did_read;           break;
        case EXB_HTTP_DID_WRITE:          ev->handle = on_http_did_write;          break;
        case EXB_HTTP_CANCEL:             ev->handle = on_http_cancel;             break;
        default: exb_assert_h(0, "invalid cmd"); break;
    }
    ev->msg.u.iip.arg1 = arg;
    ev->msg.u.iip.arg2 = cmd;
    ev->msg.u.iip.argp = object;
    return EXB_OK;
}

static inline void exb_server_on_read_available_i(struct exb_server *s, struct exb_http_multiplexer *mp) {
    struct exb_event ev;
    exb_assert_h((!!mp) && mp->state == EXB_MP_ACTIVE, "");
    exb_assert_h(!!mp->currently_reading, "");
    
    exb_event_http_init(s, mp, &ev, EXB_HTTP_READ, mp->currently_reading, 0);
    mark_read_scheduled(mp->currently_reading, 1);
    exb_evloop_append(mp->evloop, ev);
    RQSTATE_EVENT(stderr, "Scheduled rqstate %p to be read, because we found out "
                    "read is available for socket %d\n", mp->currently_reading, mp->socket_fd);
    
}
static inline void exb_server_on_write_available_i(struct exb_server *s, struct exb_http_multiplexer *mp) {
    struct exb_event ev;
    exb_event_http_init(s, mp, &ev, EXB_HTTP_SEND, mp->next_response, 0);
    exb_assert_h(!!mp->next_response, "");
    mark_send_scheduled(mp->next_response, 1);
    exb_evloop_append(mp->evloop, ev);
    
}


#endif //EXB_HTTP_SERVER_EVENTS_INTERNAL
