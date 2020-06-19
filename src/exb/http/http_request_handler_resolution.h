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
        exb_fileserv(rqstate, sink->u.fs.fs_path.str, sink->u.fs.fs_path.len, s + slice.index, slice.len);
        return EXB_OK;
    }
    else if (sink->stype == EXB_REQ_SINK_NONE) {
        return EXB_OK;
    }
    else {
        exb_assert_s(0, "invalid request sink");
    }
    return EXB_INVALID_ARG_ERR;
}

static void exb_http_request_resolve(struct exb_request_state *rqstate) {
    struct exb_server *server = rqstate->server;
    struct exb_http_server_config *config = &rqstate->server->config;
    for (int i=0; i < config->nrules; i++) {
        if (exb_http_request_matches_rule(rqstate, config->request_rules + i)) {
            exb_http_request_resolve_to_sink(rqstate, config, config->request_rules[i].sink_id);
            break;
        }
    }
}

#endif

