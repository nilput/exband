#include "http_request.h"
#include "utils.h"
#include <stdio.h>

//str name is not owned by func
static struct cpb_http_header *cpb_request_get_header(struct cpb_request_state *rqstate, struct cpb_str *name) {
    
    for (int i=0; i<rqstate->headers.len; i++) {
        struct cpb_http_header *h = &rqstate->headers.headers[i];
        if (cpb_strcasel_eq(rqstate->input_buffer+ h->key.index, h->key.len, name->str, name->len)) {
            return h;
        }
    }
    return NULL;
}
//TODO: use fast functions like memchr / strstr
static int cpb_str_next_crlf(char *s, int idx, int len) {
    if (len >= 2 && s[idx] == '\r' && s[idx+1] == '\n')
        idx += 2;
    for (int i=idx; i<len-1; i++) {
        if (s[i] == '\r' && s[i+1] == '\n')
            return i;
    }
    return -1;
}
static int cpb_str_next_lws(char *s, int idx, int len) {
    for (int i=idx; i<len-1; i++) {
        if (s[i] == ' ')
            return i;
    }
    return -1;
}
static int cpb_str_next_nonws(char *s, int idx, int len, int do_stay_in_crlf_line) {
    for (int i=idx; i<len; i++) {
        if (s[i] != ' ' && s[i] != '\t') {
            return i;
        }
        else if (s[i] == '\n')
            continue;
        else if (do_stay_in_crlf_line && (i != len - 1) && s[i] == '\r' && s[i+1] == '\n')
            return -1;
    }
    return -1;
}
static int cpb_str_crlf_line_len(char *s, int idx, int len) {
    int next_crlf = cpb_str_next_crlf(s, idx, len);
    if (next_crlf == -1)
        return len - idx;
    return next_crlf - idx;
}

static int cpb_str_has_crlfcrlf(char *s, int idx, int len) {
    return cpb_memmem(s, idx, len, "\r\n\r\n", 4) != -1;
}
static int cpb_str_find_crlfcrlf(char *s, int idx, int len) {
    return cpb_memmem(s, idx, len, "\r\n\r\n", 4);
}
static int cpb_str_is_just_preceeded_by_crlf(char *s, int idx, int len) {
    return idx >= 2 && s[idx-2] == '\r' && s[idx-1] == '\n';
}


static int cpb_request_http_parse(struct cpb_request_state *rqstate) {
    char *ibuff   =  rqstate->input_buffer;
    int ibuff_len =  rqstate->input_buffer_len;
    
    int line_len  = cpb_str_crlf_line_len(ibuff, 0, ibuff_len);
    rqstate->status_s.index = 0;
    rqstate->status_s.len = line_len;

    int first_space = cpb_str_next_lws(ibuff, 0, line_len);
    if (first_space == -1)
        return CPB_HTTP_ERROR;
    rqstate->method_s.index = 0;
    rqstate->method_s.len = first_space;
    rqstate->path_s.index = cpb_str_next_nonws(ibuff, first_space, ibuff_len, 1);
    if (rqstate->path_s.index == -1)
        return CPB_HTTP_ERROR;
    int second_space = cpb_str_next_lws(ibuff, rqstate->path_s.index, line_len - rqstate->path_s.index);
    if (second_space == -1)
        return CPB_HTTP_ERROR;
    rqstate->path_s.len = second_space - rqstate->path_s.index;
    rqstate->version_s.index = cpb_str_next_nonws(ibuff, second_space, line_len - second_space, 1);
    if (rqstate->version_s.index == -1)
        return CPB_HTTP_ERROR;
    rqstate->version_s.len = line_len - rqstate->version_s.index;

    

    int headers_begin = rqstate->status_s.index+rqstate->status_s.len + 2;
    cpb_assert_s(cpb_str_is_just_preceeded_by_crlf(ibuff, headers_begin, ibuff_len), "");
    int n_headers = 0;
    int idx = headers_begin;
    while (1) {
        //Header-Name: Value\r\n
        int line_end_idx = cpb_memmem(ibuff, idx, ibuff_len, "\r\n", 2);
        if (line_end_idx == -1) {
            //error
            return CPB_HTTP_ERROR;
        }
        int colon_idx = cpb_memmem(ibuff, idx, line_end_idx - idx, ":", 1);
        if (colon_idx == -1) {
            break;
        }
        struct cpb_str_slice key = {idx, colon_idx - idx};
        struct cpb_str_slice value = {colon_idx+1, line_end_idx - colon_idx - 1};
        idx = line_end_idx + 2; //skip crlf
        if (n_headers > CPB_HTTP_HEADER_MAX) {
            return CPB_OUT_OF_RANGE_ERR;
        }
        rqstate->headers.headers[n_headers].key = key; 
        rqstate->headers.headers[n_headers].value = value;
        n_headers++;
    }
    rqstate->headers.len = n_headers;
    cpb_assert_s(cpb_str_is_just_preceeded_by_crlf(ibuff, idx, ibuff_len), "");
    if (cpb_memmem(ibuff, idx, ibuff_len, "\r\n", 2) != idx) //crlfcrlf
        return CPB_HTTP_ERROR;
    rqstate->headers_s.index = headers_begin;
    rqstate->headers_s.len = idx - rqstate->headers_s.index - 2;
    
    rqstate->pstate = CPB_HTTP_P_ST_PARSED_HEADERS;

    cpb_request_repr(rqstate);
    
    return CPB_OK;
}
