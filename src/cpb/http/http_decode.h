#include "../cpb.h"
#include "../cpb_errors.h"
#include "../cpb_str.h"
#include "../cpb_utils.h"

static int ishexchar(int ch) {
    return (ch >= 'a' && ch <= 'f') ||  (ch >= 'A' && ch <= 'F') || (ch >= '0' && ch <= '9');
}
static int hexnibble(int ch) {
    if (ch >= '0' && ch <= '9')
        return ch - '0';
    else if (ch >= 'a' && ch <= 'f')
        ch -= 'a' - 'A';
    return ch - 'A' + 10;
}
static unsigned char hexoctet(const char *src) {
    return (hexnibble(src[0]) << 4) | hexnibble(src[1]);
}

//This assumes teh html variant which encodes spaces as '+'
//https://url.spec.whatwg.org/#concept-url-query
//https://www.w3.org/TR/xforms11/#submit-post
//^ '+' to space, note: this does not take into consideration splitting using '&' and '=', nor does it take into account ignoring
// reserved characters, all such splitting and sanitation must be done by a routing before calling this one
static int cpb_urlencode_decode(char * CPB_RESTRICT dest, int destsz, int *destlen, const char * CPB_RESTRICT src, int srclen) {
    cpb_assert_h(src != dest, "");
    cpb_assert_h(!!destlen, "");
    int dest_index = 0;
    for (int i=0; i<srclen; i++) {
        if (src[i] == '%' && i < (srclen - 2) && ishexchar(src[i+1]) && ishexchar(src[i+2])) {
            dest[dest_index++] = hexoctet(src+i+1);
            i += 2;
        }
        else if (src[i] == '+')
            dest[dest_index++] = ' ';
        else
            dest[dest_index++] = src[i];
    }
    *destlen = dest_index;
    return CPB_OK;
}

#define CPB_DECODED_PARTS_MAX 32

/*Content-Type: multipart-form-data; boundary=foo; stuff="bar"*/
//              ^^value^^^^^^^^^^^^  ^param1       ^param2
//all slices are ensured to be nul terminated
struct cpb_header_params {
    struct cpb_str buff;
    struct cpb_str_slice   value;
    struct {
        struct cpb_str_slice   keys[CPB_DECODED_PARTS_MAX];
        struct cpb_str_slice values[CPB_DECODED_PARTS_MAX];
    } params;
    int nparams;
};

//slices in this case are nul terminated
struct cpb_form_parts {
    struct cpb_str buff;
    struct cpb_str_slice   keys[CPB_DECODED_PARTS_MAX];
    struct cpb_str_slice values[CPB_DECODED_PARTS_MAX];
    int nparts;
};

static void cpb_header_params_deinit(struct cpb *cpb_ref, struct cpb_header_params *params) {
    cpb_str_deinit(cpb_ref, &params->buff);
}

static const char *skipwhitespace(const char *s, int len) {
    const char *end = s + len;
    while (s < end && (*s == ' ' || *s == '\t'))
        s++;
    return s;
}
/*
foo; bar;
   ^3
foo="hmm;whatever"; bar
                  ^18
*/
static int find_semicolon_sep(const char *haystack, int hidx, int hlen) {
    int semi_idx = cpb_memmem(haystack, 0, hlen, ";", 1);
    if (semi_idx == -1)
        return -1;
    int quote_count = 0;
    int quote_idx = hidx;
    while ((quote_idx = cpb_memmem(haystack, quote_idx, semi_idx, "\"", 1)) != -1)
        if (quote_idx == 0 || haystack[quote_idx-1] != '\\')
            quote_count++;
    if (quote_count % 2 == 1)
        return find_semicolon_sep(haystack, semi_idx+1, hlen);
    return semi_idx;
}
static int strappend_process_quotes(struct cpb *cpb_ref, struct cpb_str *buff, const char *src, int srclen, int *end_idx) {
    int rv;
    int cursor = skipwhitespace(src, srclen) - src;
    if (src[cursor] == '"') {
        cursor++;
        int end_quote_idx = cpb_memmem(src, cursor, srclen, "\"", 1);
        while (1) {
            if (end_quote_idx == -1) //unterminated
                return CPB_INVALID_ARG_ERR;
            else if (src[end_quote_idx - 1] == '\\')
                end_quote_idx = cpb_memmem(src, end_quote_idx + 1, srclen, "\"", 1);
            else
                break;
        }
        int copied_to = cursor;
        int special_idx;
        while ((special_idx = cpb_memmem(src, copied_to, srclen, "\\", 1)) != -1) {
            if ((rv = cpb_str_strlappend(cpb_ref, buff, src + copied_to, special_idx - copied_to)) != CPB_OK) {
                cpb_str_deinit(cpb_ref, buff);
                return rv;
            }
            if ((rv = cpb_str_charappend(cpb_ref, buff, src[special_idx+1])) != CPB_OK) {
                cpb_str_deinit(cpb_ref, buff);
                return rv;
            }
            copied_to = special_idx + 2;
        }
        if ((rv = cpb_str_strlappend(cpb_ref, buff, src + copied_to, end_quote_idx - copied_to)) != CPB_OK) {
            cpb_str_deinit(cpb_ref, buff);
            return rv;
        }
        *end_idx = end_quote_idx;
    }
    else {
        if ((rv = cpb_str_strlappend(cpb_ref, buff, src + cursor, srclen - cursor)) != CPB_OK) {
            cpb_str_deinit(cpb_ref, buff);
            return rv;
        }
        *end_idx = srclen;
    }
    return CPB_OK;
}

static int cpb_content_type_is(const char *content_type_header, int content_type_len, const char *mime) {
    int at = skipwhitespace(content_type_header, content_type_len) - content_type_header;
    if (content_type_len - at < strlen(mime))
        return 0;
    return cpb_strcasel_eq(content_type_header + at, strlen(mime), mime, strlen(mime)) &&
           ((at + strlen(mime) + 1 > content_type_len) || content_type_header[at+1+strlen(mime)] == ';' || content_type_header[at+1+strlen(mime)] == ' ');
}

//params_out is assumed to be uninitialized
static int cpb_decode_header_params(struct cpb *cpb_ref, struct cpb_header_params *params_out, const char *src, int srclen) {
    params_out->nparams = 0;
    params_out->value.index = 0;
    params_out->value.len = 0;
    cpb_str_init_empty(&params_out->buff);
    int cursor = skipwhitespace(src, srclen) - src;
    
    int sep_idx = find_semicolon_sep(src, cursor, srclen);
    if (sep_idx == -1) {
        return CPB_OK;
    }
    int rv;
    if ((rv = cpb_str_strlappend(cpb_ref, &params_out->buff, src + cursor, sep_idx - cursor)) != CPB_OK) {
        cpb_str_deinit(cpb_ref, &params_out->buff);
        return rv;
    }
    cpb_str_rtrim(cpb_ref, &params_out->buff);
    params_out->value.len = params_out->buff.len;
    int part_end = 0;
    do {
        cursor = sep_idx + 1;
        sep_idx = cpb_memmem(src, cursor, srclen, ";", 1);
        part_end = sep_idx == -1 ? srclen : sep_idx;

        int eq_idx = cpb_memmem(src, cursor, part_end, "=", 1);
        if (eq_idx == -1)
            continue; //silent error handling!
        
        params_out->buff.len++; //keep old nul terminator
        int key_index = params_out->buff.len;
        int remainder_idx;
        if ((rv = strappend_process_quotes(cpb_ref, &params_out->buff, src + cursor, eq_idx - cursor, &remainder_idx)) != CPB_OK) {
            cpb_str_deinit(cpb_ref, &params_out->buff);
            return rv;
        }
        cpb_str_rtrim(cpb_ref, &params_out->buff);
        int key_len = params_out->buff.len - key_index;


        params_out->buff.len++; //keep old nul terminator
        int value_index = params_out->buff.len;
        if ((rv = strappend_process_quotes(cpb_ref, &params_out->buff, src + eq_idx + 1, part_end - eq_idx - 1, &remainder_idx)) != CPB_OK) {
            cpb_str_deinit(cpb_ref, &params_out->buff);
            return rv;
        }
        cpb_str_rtrim(cpb_ref, &params_out->buff);
        int value_len = params_out->buff.len - value_index;

        params_out->params.keys[params_out->nparams].index = key_index;
        params_out->params.keys[params_out->nparams].len   = key_len;
        params_out->params.values[params_out->nparams].index = value_index;
        params_out->params.values[params_out->nparams].len   = value_len;
        params_out->nparams++;
    } while (part_end < srclen);
    return CPB_OK;    
}


static void cpb_form_parts_deinit(struct cpb *cpb_ref, struct cpb_form_parts *parts) {
    cpb_str_deinit(cpb_ref, &parts->buff);
}
//assumes parts_out is uninitialized
static int cpb_urlencode_decode_parts(struct cpb *cpb_ref, struct cpb_form_parts *parts_out, const char * CPB_RESTRICT src, int srclen) {
    memset(parts_out, 0, sizeof *parts_out);
    cpb_str_init_empty(&parts_out->buff);
    int rv;
    int part_begin = 0;
    do {
        int sep_idx = cpb_memmem(src, part_begin, srclen, "&", 1);
        int part_end = sep_idx == -1 ? srclen : sep_idx;
        int eq_idx = cpb_memmem(src, part_begin, srclen, "=", 1);
        if (eq_idx != -1) {
            //otherwise if '=' is not there then be permissive and silently ignore this part
            if (parts_out->nparts + 1 >= CPB_DECODED_PARTS_MAX) {
                cpb_str_deinit(cpb_ref, &parts_out->buff);
                return CPB_OUT_OF_RANGE_ERR;
            }
            if ((rv = cpb_str_ensure_cap(cpb_ref, &parts_out->buff, parts_out->buff.len + (part_end - part_begin) + 2)) != CPB_OK) {
                cpb_str_deinit(cpb_ref, &parts_out->buff);
                return rv;
            }
            int written = 0;
            //decode key
            rv = cpb_urlencode_decode(parts_out->buff.str + parts_out->buff.len + 1, //dest
                                        parts_out->buff.cap - parts_out->buff.len - 1, //destsz
                                        &written, //destlen
                                        src + part_begin, //src
                                        eq_idx - part_begin); //srclen
            if (rv != CPB_OK) {
                cpb_str_deinit(cpb_ref, &parts_out->buff);
                return rv;
            }
            //key1\0value1\0key2\0value2\0 ...
            parts_out->keys[parts_out->nparts].index = parts_out->buff.len + 1;
            parts_out->keys[parts_out->nparts].len = written;
            parts_out->buff.len += 1 + written;
                                // ^ previous nul terminator
            parts_out->buff.str[parts_out->buff.len] = 0;
            //decode value
            rv = cpb_urlencode_decode(parts_out->buff.str + parts_out->buff.len + 1, //dest
                                        parts_out->buff.cap - parts_out->buff.len - 1, //destsz
                                        &written, //destlen
                                        src + eq_idx + 1, //src
                                        part_end - eq_idx - 1); //srclen
            if (rv != CPB_OK) {
                cpb_str_deinit(cpb_ref, &parts_out->buff);
                return rv;
            }
            parts_out->values[parts_out->nparts].index = parts_out->buff.len + 1;
            parts_out->values[parts_out->nparts].len = written;
            parts_out->buff.len += 1 + written;
                                // ^ previous nul terminator
            parts_out->buff.str[parts_out->buff.len] = 0;

            parts_out->nparts++;
        }
        part_begin = part_end + 1;
    } while (part_begin < srclen);
    return CPB_OK;
}

static int cpb_cat_memmem( const char *haystack, int hidx, int hlen,
                    const char *beg, int beglen,
                    const char *mid, int midlen,
                    const char *end, int endlen )
{
    int mididx = cpb_memmem(haystack, hidx, hlen, mid, midlen);
    if (mididx == -1)
        return -1;
    if ((mididx - beglen >= hidx)                                            &&
        (mididx + midlen + endlen <= hlen)                                   &&
        (!beglen || (memcmp(beg, haystack + mididx - beglen, beglen) == 0))  &&
        (!endlen || (memcmp(end, haystack + mididx + midlen, endlen) == 0))   )
        return mididx - beglen;
    return cpb_cat_memmem(haystack, mididx + 1, hlen, beg, beglen, mid, midlen, end, endlen);
    
}
/*
note: parts_out->keys slicesr base is parts_out->buff.str
     but 
     parts_out->values slice base is body
     this is to avoid copying possibly large multipart bodies
*/
static int cpb_decode_multipart(struct cpb *cpb_ref, struct cpb_form_parts *parts_out, const char *content_type_header, int content_type_len, const char * body, int bodylen) {
    memset(parts_out, 0, sizeof *parts_out);
    cpb_str_init_empty(&parts_out->buff);
    
    int rv;
    struct cpb_header_params hp;
    if ((rv = cpb_decode_header_params(cpb_ref, &hp, content_type_header, content_type_len)) != CPB_OK) {
        return rv;
    }
    char *boundary = NULL;
    int boundary_len;
    for (int i=0; i<hp.nparams; i++) {
        if (strcmp("boundary", hp.buff.str + hp.params.keys[i].index) == 0) {
            boundary = hp.buff.str + hp.params.values[i].index;
            boundary_len = hp.params.values[i].len;
            break;
        }
    }
    if (boundary == NULL) {
        cpb_header_params_deinit(cpb_ref, &hp);
        cpb_form_parts_deinit(cpb_ref, parts_out);
        return CPB_INVALID_ARG_ERR;
    }

    int first_boundary = cpb_cat_memmem(body, 0, bodylen,
                                         "--", 2,
                                        boundary, boundary_len,
                                        "\r\n", 2);
    if (first_boundary == -1) {
        cpb_header_params_deinit(cpb_ref, &hp);
        cpb_form_parts_deinit(cpb_ref, parts_out);
        return CPB_INVALID_ARG_ERR;
    }
    int part_begin = first_boundary + 2 + boundary_len + 2;
    int part_end;
    int next_part;
    do {
        next_part = cpb_cat_memmem(body, part_begin, bodylen,
                                         "\r\n--", 4,
                                        boundary, boundary_len,
                                        "\r\n", 2);
        if (next_part == -1) {
            //final part
            part_end = cpb_cat_memmem(body, part_begin, bodylen,
                                        "\r\n--", 4,
                                        boundary, boundary_len,
                                        "--", 2);
            if (part_end == -1) {
                cpb_header_params_deinit(cpb_ref, &hp);
                cpb_form_parts_deinit(cpb_ref, parts_out);
                return CPB_INVALID_ARG_ERR;
            }
        }
        else {
            part_end = next_part;
        }
        //process headers
        
        int headers_end = cpb_memmem(body, part_begin, part_end, "\r\n\r\n", 4);
        if (headers_end == -1) {
            cpb_header_params_deinit(cpb_ref, &hp);
            cpb_form_parts_deinit(cpb_ref, parts_out);
            return CPB_INVALID_ARG_ERR;
        }
        int next_line;
        struct cpb_str part_name;
        int current_header = part_begin;
        int current_header_end = part_begin;
        cpb_str_init_empty(&part_name);
        do {
            next_line = cpb_memmem(body, current_header, headers_end, "\r\n", 2);
            if (next_line == -1) {
                current_header_end = headers_end;
            }
            else {
                current_header_end = next_line;
            }
            int colon_idx = cpb_memmem(body, current_header, current_header_end, ":", 1);
            if (colon_idx == -1) {
                cpb_header_params_deinit(cpb_ref, &hp);
                cpb_form_parts_deinit(cpb_ref, parts_out);
                cpb_str_deinit(cpb_ref, &part_name);
                return CPB_INVALID_ARG_ERR;
            }
            struct cpb_str_slice name  = {current_header, colon_idx - current_header};
            struct cpb_str_slice value = {colon_idx+1, current_header_end - colon_idx - 1};
            cpb_str_slice_trim(body, &name);
            cpb_str_slice_trim(body, &value);
            if (cpb_strcasel_eq(body + name.index, name.len, "Content-Disposition", 19)) {
                struct cpb_header_params dp;
                if ((rv = cpb_decode_header_params(cpb_ref, &dp, body + value.index, value.len)) != CPB_OK) {
                    cpb_header_params_deinit(cpb_ref, &hp);
                    cpb_form_parts_deinit(cpb_ref, parts_out);
                    return CPB_INVALID_ARG_ERR;
                }
                for (int i=0; i<dp.nparams; i++) {
                    if (cpb_strcasel_eq(dp.buff.str + dp.params.keys[i].index, dp.params.keys[i].len, "name", 4)) {
                        if ((rv = cpb_str_init_strlcpy(cpb_ref,
                                                        &part_name,
                                                        dp.buff.str + dp.params.values[i].index,
                                                         dp.params.values[i].len)) != CPB_OK)
                        {
                            cpb_header_params_deinit(cpb_ref, &hp);
                            cpb_header_params_deinit(cpb_ref, &dp);
                            cpb_form_parts_deinit(cpb_ref, parts_out);
                            cpb_str_deinit(cpb_ref, &part_name);
                            return CPB_INVALID_ARG_ERR;        
                        }
                        break;
                    }
                }
                cpb_header_params_deinit(cpb_ref, &dp);
            }
        } while (next_line != -1);
        
        if (parts_out->nparts + 1 > CPB_DECODED_PARTS_MAX) {
            cpb_header_params_deinit(cpb_ref, &hp);
            cpb_form_parts_deinit(cpb_ref, parts_out);
            cpb_str_deinit(cpb_ref, &part_name);
            return CPB_OUT_OF_RANGE_ERR;
        }
        parts_out->buff.len++; //keep old nul terminator
        parts_out->keys[parts_out->nparts].index = parts_out->buff.len;
        if ((rv = cpb_str_strlappend(cpb_ref, &parts_out->buff, part_name.str, part_name.len)) != CPB_OK) {
            cpb_header_params_deinit(cpb_ref, &hp);
            cpb_form_parts_deinit(cpb_ref, parts_out);
            cpb_str_deinit(cpb_ref, &part_name);
            return rv;
        }
        parts_out->keys[parts_out->nparts].len = parts_out->buff.len - parts_out->keys[parts_out->nparts].index;
        cpb_str_deinit(cpb_ref, &part_name);

        //BASED ON BODY STR
        parts_out->values[parts_out->nparts].index = headers_end + 4 /*crlfcrlf*/;
        parts_out->values[parts_out->nparts].len   = part_end - parts_out->values[parts_out->nparts].index;
        
        parts_out->nparts++;
        part_begin = next_part + 4 + boundary_len + 2;
    } while (next_part != -1);
    
    return CPB_OK;
}