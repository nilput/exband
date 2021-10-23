#ifndef EXB_HTTP_REQUEST_HANDLER_RESOLVE_H
#define EXB_HTTP_REQUEST_HANDLER_RESOLVE_H
#include "http_request.h"
#include "http_response.h"
#include "http_server_config.h"
#include "http_server.h"
#include "../exb_errors.h"
#include "exb_fileserv.h"

static int exb_http_request_matches_rule(struct exb_request_state *rqstate,
                                         struct exb_request_rule *rule)
{
    if (rule->type == EXB_REQ_RULE_PATH_PREFIX) {
        struct exb_str_slice slice;
        char *s  = NULL;
        exb_request_get_path_slice(rqstate, &s, &slice);
        if (slice.len >= rule->u.prefix_rule.prefix.len) {
            if (memcmp(s + slice.index, rule->u.prefix_rule.prefix.str, rule->u.prefix_rule.prefix.len) == 0) {
                return 1;
            }
        }
    }
    else if (rule->type == EXB_REQ_RULE_NONE) {
        return 0;
    }
    else {
        exb_assert_s(0, "invalid request rule");
    }
    return 0;
}

static int exb_http_request_resolve_to_sink(struct exb_request_state *rqstate,
                                            struct exb_http_server_config *config,
                                            int sink_id)
{
    struct exb_request_sink *sink = config->request_sinks + sink_id;
    if (sink->stype == EXB_REQ_SINK_FILESYSTEM) {
        struct exb_str_slice slice;
        char *s  = NULL;
        
        exb_request_get_path_slice(rqstate, &s, &slice);
        if (sink->u.fs.alias_len > slice.len)
            return EXB_INTERNAL_ERROR;
        // Here we keep the length of the alias in the sink struct, so that it's skipped
        // for example suppose you aliased "/files/" to "/usr/www/stuff/", the length of the alias 7 is stored
        // a request with "/files/hello.txt" is made, we strip the first 6 characters of the request (excluding final '/')
        // it becomes: "/hello.txt"  with the effective root being at /usr/www/stuff/, it's resolved to /usr/www/stuff/hello.txt
        char  *resource_path     = s + slice.index + sink->u.fs.alias_len;
        size_t resource_path_len = slice.len - sink->u.fs.alias_len;
        return exb_fileserv(rqstate,
                            sink->u.fs.fs_path.str,
                            sink->u.fs.fs_path.len,
                            resource_path,
                            resource_path_len);
    }
    else if (sink->stype == EXB_REQ_SINK_NONE) {
        return EXB_OK;
    }
    else if (sink->stype == EXB_REQ_SINK_FPTR) {
        exb_assert_h(!!sink->u.fptr.func, "invalid fptr request sink");
        rqstate->rqh_state       = sink->u.fptr.rqh_state;
        rqstate->request_handler = sink->u.fptr.func;
        return EXB_OK;
    }
    else {
        exb_assert_s(0, "invalid request sink");
    }
    return EXB_INVALID_ARG_ERR;
}

//returns -1 if not found
static int exb_http_get_domain_index(struct exb_server *server, char *domain, size_t domain_len) {
    //TODO: implement vhosts
    for (int i=0; i < server->config.n_domains; i++) {
        struct exb_str *name = &server->config.domains[i].server_name;
        if (name->len == domain_len && memcmp(domain, name->str, domain_len) == 0)
            return i;
    }
    return 0;
}

static int exb_http_request_resolve_domain(struct exb_request_state *rqstate) {
    if (rqstate->headers.h_host_idx >= 0) {
        struct exb_str_slice slice = rqstate->headers.headers[rqstate->headers.h_host_idx].value;
        int domain_id = exb_http_get_domain_index(rqstate->server,
                                                  rqstate->input_buffer + slice.index,
                                                  slice.len);
        if (domain_id >= 0)
            return domain_id;
    }
    //TODO: set default to default server index (or just move the default server to be id 0)
    return 0;
}

static void exb_http_request_resolve(struct exb_request_state *rqstate) {
    struct exb_server *server = rqstate->server;
    struct exb_http_server_config *config = &rqstate->server->config;
    int domain_id = exb_http_request_resolve_domain(rqstate);
    for (int i = config->domains[domain_id].rules_begin; i < config->domains[domain_id].rules_end; i++) {
        if (exb_http_request_matches_rule(rqstate, config->request_rules + i)) {
            int sink_id = config->request_rules[i].sink_id;
            {
                struct exb_str s;
                int rv = exb_request_get_path_copy(rqstate, &s);
                exb_logger_logf(rqstate->server->exb,
                                EXB_LOG_DEBUG,
                                "Request '%s' resolved to sink_id: %d by rule_id: %d domain_id: %d\n", s.str, sink_id, i, domain_id);
                exb_str_deinit(rqstate->server->exb, &s);
            }
            exb_http_request_resolve_to_sink(rqstate, config, sink_id);
            return;
        }
    }

    {
        struct exb_str s;
        int rv = exb_request_get_path_copy(rqstate, &s);
        exb_logger_logf(rqstate->server->exb,
                        EXB_LOG_DEBUG,
                        "Request '%s' resolved to default handler, domain_id: %d\n", s.str, domain_id);
    }
}

#endif

