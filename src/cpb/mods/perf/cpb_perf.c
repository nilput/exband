#include "../../cpb.h"
#include "../../http/http_server_module.h"
#include "../../http/http_request.h"
#include "../../http/http_server.h"
#include "cpb_event_perf_gen.h"
#include "cpb_event_perf_act.h"

struct perf_module {
    struct cpb_http_server_module head;
    struct cpb *cpb_ref;
    struct cpb_perf_gen gen;
    int count;
};

static void destroy_module(struct cpb_http_server_module *module, struct cpb *cpb) {
    cpb_free(cpb, module);
}
int cpb_perf_init(struct cpb *cpb, struct cpb_server *server, char *module_args, struct cpb_http_server_module **module_out) {
    (void) module_args;
    struct perf_module *mod = cpb_malloc(cpb, sizeof(struct perf_module));
    if (!mod)
        return CPB_NOMEM_ERR;
    mod->cpb_ref = cpb;
    
    mod->head.destroy = destroy_module;
    mod->count = 0;

    cpb_perf_gen_init(&mod->gen, cpb_server_get_any_eloop(server), 1000000);
    cpb_assert_h(mod->gen.eloop, "");
    cpb_perf_gen_begin(&mod->gen);
    
    *module_out = (struct cpb_http_server_module*)mod;
    return CPB_OK;
}
