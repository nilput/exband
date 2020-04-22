#ifndef EXB_HTTT_PARSE_H 
#define EXB_HTTT_PARSE_H 
#include "http_request.h"
#include "http_server.h"
#include "../exb_utils.h"
#include "../exb_str.h"
#include <ctype.h>
#include <stdio.h>

//str name is not owned by func
static struct exb_http_header *exb_request_get_header(struct exb_request_state *rqstate, struct exb_str *name) {
    
    for (int i=0; i<rqstate->headers.len; i++) {
        struct exb_http_header *h = &rqstate->headers.headers[i];
        if (exb_strcasel_eq(rqstate->input_buffer+ h->key.index, h->key.len, name->str, name->len)) {
            return h;
        }
    }
    return NULL;
}
//TODO: use fast functions like memchr / strstr
static inline int exb_str_next_crlf(char *s, int idx, int len) {
    if (len >= 2 && s[idx] == '\r' && s[idx+1] == '\n')
        idx += 2;
    return exb_memmem(s, idx, len, "\r\n", 2);
}
//it seems using memchr is slower because we're dealing with small distances until next match
static inline int exb_str_next_lws(char *s, int idx, int len) {
    return exb_memchr(s, idx, len, ' ');
}
static inline int exb_str_next_nonws(char *s, int idx, int len, int do_stay_in_crlf_line) {
    for (int i=idx; i<(idx+len); i++) {
        if (s[i] != ' ' && s[i] != '\t') {
            return i;
        }
        else if (s[i] == '\n')
            continue;
        else if (do_stay_in_crlf_line && (i <= (idx+len - 2)) && s[i] == '\r' && s[i+1] == '\n')
            return -1;
    }
    return -1;
}
static inline int exb_str_crlf_line_len(char *s, int idx, int len) {
    int next_crlf = exb_str_next_crlf(s, idx, len);
    if (next_crlf == -1)
        return len - idx;
    return next_crlf - idx;
}

static int exb_str_has_crlfcrlf(char *s, int idx, int len) {
    return exb_memmem(s, idx, len, "\r\n\r\n", 4) != -1;
}
static int exb_str_find_crlfcrlf(char *s, int idx, int len) {
    return exb_memmem(s, idx, len, "\r\n\r\n", 4);
}
static int exb_str_is_just_preceeded_by_crlf(char *s, int idx, int len) {
    return idx >= 2 && s[idx-2] == '\r' && s[idx-1] == '\n';
}

static void process_relevant_header(struct exb_request_state *rqstate, char *ibuff, struct exb_str_slice key, struct exb_str_slice value, int idx) {
    if (exb_strcasel_eq(ibuff + key.index, key.len, "Content-Length", 14)) {
        rqstate->headers.h_content_length_idx = idx;
    }
    else if (exb_strcasel_eq(ibuff + key.index, key.len, "Content-Type", 12)) {
        rqstate->headers.h_content_type_idx = idx;
    }
    else if (exb_strcasel_eq(ibuff + key.index, key.len, "Transfer-Encoding", 17)) {
        rqstate->headers.h_transfer_encoding_idx = idx;
    }
    else if (exb_strcasel_eq(ibuff + key.index, key.len, "Connection", 10)) {
        rqstate->headers.h_connection_idx = idx;
    }
}

static void exb_unknown_standard_header_value(struct exb_request_state *rqstate, int header_idx) {

}

static int exb_request_http_parse_chunked_encoding(struct exb_request_state *rqstate) {
    exb_assert_s(rqstate->is_chunked, "");
    char *ibuff = rqstate->input_buffer;
    int ibuff_len = rqstate->input_buffer_len;
    if (rqstate->pstate != EXB_HTTP_P_ST_IN_CHUNKED_BODY) {
        return EXB_INVALID_STATE_ERR;
    }
    if (rqstate->istate != EXB_HTTP_I_ST_WAITING_FOR_BODY) {
        return EXB_INVALID_STATE_ERR;
    }
    
    while (rqstate->parse_chunk_cursor < ibuff_len) {
        
        int chunk_len;
        int chunk_digits_len = exb_atoi_hex_rlen(ibuff     + rqstate->parse_chunk_cursor,
                                                 ibuff_len - rqstate->parse_chunk_cursor,
                                                 &chunk_len);
        if (chunk_digits_len == 0) {
            return EXB_HTTP_ERROR;
        }
        int chunk_begin_idx = rqstate->parse_chunk_cursor + chunk_digits_len + 2; //2 for crlf
        int chunk_end_idx   = chunk_begin_idx + chunk_len;
        
        if ((chunk_end_idx + 2) > ibuff_len)
            return EXB_OK; //haven't read enough for after-data crlf
        if (memcmp(ibuff+rqstate->parse_chunk_cursor + chunk_digits_len, "\r\n", 2) != 0) {
            return EXB_HTTP_ERROR;
        }
        if (chunk_len == 0) {
            if ((chunk_end_idx + 2) > ibuff_len)
                return EXB_OK; //havent read enough for the final crlfcrlf
            //here chunk_begin == chunk_end
            if (memcmp(ibuff+chunk_end_idx, "\r\n", 2) != 0)
                return EXB_HTTP_ERROR;
            rqstate->pstate = EXB_HTTP_P_ST_DONE;
            rqstate->next_request_cursor = chunk_end_idx + 2;
        }
        rqstate->parse_chunk_cursor = chunk_end_idx + 2;

        //TODO: this data probably shouldn't be stored in the input buffer, ALSO it should be used (streamed to handler or smth.) or discard
        //Depending on rqstate->body_handling
        //Currently we're in all cases saving it undecoded in input buffer, then discarding it, or accumulating it in ->decoded_body
        if (rqstate->body_handling == EXB_HTTP_B_BUFFER) {
            int rv = exb_str_strlappend(rqstate->server->exb, &rqstate->body_decoded, rqstate->input_buffer + chunk_begin_idx, chunk_len);
            if (rv != EXB_OK) {
                return rv;
            }
        }
        else if (rqstate->body_handling != EXB_HTTP_B_DISCARD) {
            exb_assert_h(0, "");
        }
        
    }
    return EXB_OK;
}

static inline int exb_isdigit(int c) {
    return c >= '0' && c <= '9';
}
static struct exb_error exb_request_http_parse(struct exb_request_state *rqstate) {
    char *ibuff   =  rqstate->input_buffer;
    int ibuff_len =  rqstate->input_buffer_len;
    
    int line_len  = exb_str_crlf_line_len(ibuff, 0, ibuff_len);
    rqstate->status_s.index = 0;
    rqstate->status_s.len = line_len;

    int first_space = exb_str_next_lws(ibuff, 0, line_len);
    if (first_space == -1)
        return exb_make_error(EXB_HTTP_ERROR);
    rqstate->method_s.index = 0;
    rqstate->method_s.len = first_space;

    //TODO: optimize
    struct method_lookup { int method; char *name; int namelen; };
    struct method_lookup methods_lookup[] = {
        {EXB_HTTP_M_GET,      "get",     3},
        {EXB_HTTP_M_POST,     "post",    4},
        {EXB_HTTP_M_PUT,      "put",     3},
        {EXB_HTTP_M_HEAD,     "head",    4},
        {EXB_HTTP_M_PATCH,    "patch",   5},
        {EXB_HTTP_M_DELETE,   "delete",  6},
        {EXB_HTTP_M_TRACE,    "trace",   5},
        {EXB_HTTP_M_OPTIONS,  "options", 7},
    };
    int methods_lookup_len = sizeof(methods_lookup) / sizeof(struct method_lookup);
    rqstate->method = EXB_HTTP_M_OTHER;
    for (int i=0; i<methods_lookup_len; i++) {
        if (rqstate->method_s.len == methods_lookup[i].namelen &&
            exb_strcasel_eq(ibuff + rqstate->method_s.index, rqstate->method_s.len, methods_lookup[i].name, methods_lookup[i].namelen)) {
            rqstate->method = methods_lookup[i].method;
            break;
        }
    }
    
    rqstate->path_s.index = exb_str_next_nonws(ibuff, first_space, ibuff_len, 1);
    if (rqstate->path_s.index == -1)
        return exb_make_error(EXB_HTTP_ERROR);
    int second_space = exb_str_next_lws(ibuff, rqstate->path_s.index, line_len - rqstate->path_s.index);
    if (second_space == -1)
        return exb_make_error(EXB_HTTP_ERROR);
    rqstate->path_s.len = second_space - rqstate->path_s.index;
    rqstate->version_s.index = exb_str_next_nonws(ibuff, second_space, line_len - second_space, 1);
    if (rqstate->version_s.index == -1)
        return exb_make_error(EXB_HTTP_ERROR);
    rqstate->version_s.len = line_len - rqstate->version_s.index;
    if ( rqstate->version_s.len < 8                                       ||
        !exb_strcasel_eq(ibuff + rqstate->version_s.index, 5, "HTTP/", 5) ||
        !exb_isdigit(ibuff[rqstate->version_s.index + 5])                     ||
         ibuff[rqstate->version_s.index + 6] != '.'                       ||
        !exb_isdigit(ibuff[rqstate->version_s.index + 7])                       )
    {
        return exb_make_error(EXB_HTTP_ERROR);
    }
    //http/x.x
    rqstate->http_major = ibuff[rqstate->version_s.index+5] - '0';
    rqstate->http_minor = ibuff[rqstate->version_s.index+7] - '0';

    if (rqstate->http_major > 1 || rqstate->http_minor > 1)
        return exb_make_error(EXB_HTTP_ERROR);

    int headers_begin = rqstate->status_s.index+rqstate->status_s.len + 2;
    exb_assert_s(exb_str_is_just_preceeded_by_crlf(ibuff, headers_begin, ibuff_len), "");
    int n_headers = 0;
    int idx = headers_begin;
    while (1) {
        //Header-Name: Value\r\n
        int line_end_idx = exb_memmem(ibuff, idx, ibuff_len, "\r\n", 2);
        if (line_end_idx == -1) {
            //error
            return exb_make_error(EXB_HTTP_ERROR);
        }
        int colon_idx = exb_memmem(ibuff, idx, line_end_idx, ":", 1);
        if (colon_idx == -1) {
            break;
        }
        struct exb_str_slice key = {idx, colon_idx - idx};
        struct exb_str_slice value = {colon_idx+1, line_end_idx - colon_idx - 1};

        int nonws = exb_str_next_nonws(ibuff, value.index, value.len, 1);
        if (nonws != -1) {
            value.len -= nonws - value.index;
            value.index = nonws;
        }
        

        idx = line_end_idx + 2; //skip crlf
        if (n_headers > EXB_HTTP_HEADER_MAX) {
            return exb_make_error(EXB_HTTP_ERROR);
        }
        rqstate->headers.headers[n_headers].key = key; 
        rqstate->headers.headers[n_headers].value = value;

        n_headers++;
        process_relevant_header(rqstate, ibuff, key, value, n_headers-1);
    }
    rqstate->headers.len = n_headers;
    exb_assert_s(exb_str_is_just_preceeded_by_crlf(ibuff, idx, ibuff_len), "");
    if (exb_memmem(ibuff, idx, ibuff_len, "\r\n", 2) != idx) //crlfcrlf
        return exb_make_error(EXB_HTTP_ERROR);
    rqstate->headers_s.index = headers_begin;
    rqstate->headers_s.len = idx - rqstate->headers_s.index - 2;
    rqstate->body_s.index = idx + 2; //after the crlf

    if (rqstate->http_minor == 0)
        rqstate->is_persistent = 0;
    else if (rqstate->http_minor == 1)
        rqstate->is_persistent = 1;
    
    if (rqstate->headers.h_connection_idx != -1) {
        struct exb_str_slice value = rqstate->headers.headers[rqstate->headers.h_connection_idx].value;
        if (exb_strcasel_eq(ibuff + value.index, value.len, "close", 5)) {
            rqstate->is_persistent = 0;
        }
        else if (exb_strcasel_eq(ibuff + value.index, value.len, "keep-alive", 10)) {
            rqstate->is_persistent = 1;
        }
        else {
            exb_unknown_standard_header_value(rqstate, rqstate->headers.h_connection_idx);
        }
    }
    if (rqstate->headers.h_content_length_idx != -1) {
        struct exb_str_slice value = rqstate->headers.headers[rqstate->headers.h_content_length_idx].value;
        int len;
        int err = exb_atoi(ibuff + value.index, value.len, &len);
        if (err != EXB_OK || len < 0) {
            return exb_make_error(EXB_HTTP_ERROR);
        }
        rqstate->content_length = len;
        rqstate->body_s.len = len;
    }
    if (rqstate->headers.h_transfer_encoding_idx != -1) {
        struct exb_str_slice value = rqstate->headers.headers[rqstate->headers.h_transfer_encoding_idx].value;
        if (exb_strcasel_eq(ibuff + value.index, value.len, "identity", 8)) {
            rqstate->is_chunked = 0;
        }
        else if (exb_strcasel_eq(ibuff + value.index, value.len, "chunked", 7)) {
            rqstate->is_chunked = 1;
        }
        else {
            exb_unknown_standard_header_value(rqstate, rqstate->headers.h_transfer_encoding_idx);
        }
    }
    
    if (rqstate->is_chunked) {
        rqstate->pstate = EXB_HTTP_P_ST_IN_CHUNKED_BODY;
    }
    else {
        rqstate->pstate = EXB_HTTP_P_ST_DONE;
    }
    
    
    return exb_make_error(EXB_OK);
}

#endif // EXB_HTTT_PARSE_H 