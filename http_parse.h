#include "http_request.h"
#include "utils.h"
#include <stdio.h>

//str name is not owned by func
static struct cpb_http_header *cpb_request_get_header(struct cpb_request_state *rstate, struct cpb_str *name) {
    
    for (int i=0; i<rstate->headers.len; i++) {
        struct cpb_http_header *h = &rstate->headers.headers[i];
        if (cpb_strcasel_eq(rstate->input_buffer+ h->key_idx, h->key_len, name->str, name->len)) {
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


static int cpb_request_http_parse_headers(struct cpb_request_state *rstate) {
}

static int cpb_request_http_parse(struct cpb_request_state *rstate) {
    
    struct cpb_str_slice method;
    struct cpb_str_slice path;
    struct cpb_str_slice version;
    char *ibuff   =  rstate->input_buffer;
    int ibuff_len =  rstate->input_buffer_len;
    int line_len  = cpb_str_crlf_line_len(ibuff, 0, ibuff_len);
    int first_space = cpb_str_next_lws(ibuff, 0, line_len);
    if (first_space == -1)
        return CPB_HTTP_ERROR;
    method.index = 0;
    method.len = first_space;
    path.index = cpb_str_next_nonws(ibuff, first_space, ibuff_len, 1);
    if (path.index == -1)
        return CPB_HTTP_ERROR;
    int second_space = cpb_str_next_lws(ibuff, path.index, line_len - path.index);
    if (second_space == -1)
        return CPB_HTTP_ERROR;
    path.len = second_space - path.index;
    version.index = cpb_str_next_nonws(ibuff, path.index+path.len, line_len - (path.index+path.len), 1);
    if (version.index == -1)
        return CPB_HTTP_ERROR;
    version.len = line_len - version.index;
    rstate->method_s = method;
    rstate->path_s = path;
    rstate->version_s = version;

    cpb_request_repr(rstate);
    

    
    return NULL;
}
