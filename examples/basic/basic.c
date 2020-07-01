#include "exb/exb.h"
#include "exb/http/http_server_module.h"
#include "exb/http/http_request.h"

struct basic_module {
    struct exb_http_server_module head;
    struct exb *exb_ref;
    int count;
};
int basic_handle_request(void *rqh_state, struct exb_request_state *rqstate, int reason) {
    struct basic_module *mod = (struct basic_module *) rqh_state;

    struct exb_str path;
    //exb_request_repr(rqstate);

    exb_str_init_empty(&path);
    exb_str_slice_to_copied_str(mod->exb_ref, rqstate->path_s, rqstate->input_buffer, &path);
    struct exb_str key,value;
    exb_str_init_const_str(&key, "Content-Type");
    exb_str_init_const_str(&value, "text/plain");
    exb_response_set_header(rqstate, &key, &value);
    exb_response_append_body(rqstate, "Hello World!\r\n", 14);
    exb_response_end(rqstate);
    exb_str_deinit(mod->exb_ref, &path);   
    return 0;
}
static void destroy_module(struct exb_http_server_module *module, struct exb *exb_ref) {
    exb_free(exb_ref, module);
}
int handler_init(struct exb *exb_ref, struct exb_server *server, char *module_args, struct exb_http_server_module **module_out) {
    (void) module_args;
    struct basic_module *mod = exb_malloc(exb_ref, sizeof(struct basic_module));
    if (!mod)
        return EXB_NOMEM_ERR;
    mod->exb_ref = exb_ref;
    mod->head.destroy = destroy_module;
    mod->count = 0;
    
    *module_out = (struct exb_http_server_module*)mod;
    return EXB_OK;
}
