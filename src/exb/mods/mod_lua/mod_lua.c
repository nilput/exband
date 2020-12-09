#include "exb/exb.h"
#include "exb/http/http_server_module.h"
#include "exb/http/http_request.h"

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

//TODO: have them dynamically allocated
#define MAX_CPU_COUNT 8

//Kept for each cpu
struct lua_instance {
    lua_State *L; //If NULL then it's currently uninitalized
};

struct lua_module {
    struct exb_http_server_module head;
    struct exb *exb_ref;
    struct lua_instance lua_instances[MAX_CPU_COUNT];
};

static int lua_instance_init(struct exb *exb_ref, struct lua_instance *li) {
    lua_State *L = luaL_newstate();    
    if (!L)
        return EXB_INIT_ERROR;
    luaL_openlibs(L);
    li->L = L;
    if (luaL_loadfile(li->L, "script.lua") != 0) {
        exb_log_error(exb_ref, "Failed to load lua script");
        lua_close(li->L);
        li->L = NULL;
        return EXB_INIT_ERROR;
    }
    //call it once to initialize stuff
    if (lua_pcall(li->L, 0, 0, 0) != 0) {
        exb_log_error(exb_ref, "Calling lua script resulted in an error");
        lua_close(li->L);
        li->L = NULL;
        return EXB_INIT_ERROR;
    }
    return EXB_OK;
}

int exb_lua_handle_request(void *rqh_state, struct exb_request_state *rqstate, int reason) {
    struct lua_module *mod = (struct lua_module *) rqh_state;
    int eloop_idx = exb_request_get_eloop_index(rqstate);
    if (eloop_idx > MAX_CPU_COUNT) {
        exb_log_error(mod->exb_ref, "Failed to initialize lua instance for eloop_idx: %d,"
                                    " cause: eloop_idx > MAX_CPU_COUNT\n", eloop_idx);
        return EXB_INIT_ERROR;
    }
    int rv = 0;
    if (!mod->lua_instances[eloop_idx].L) {
        rv = lua_instance_init(mod->exb_ref, mod->lua_instances + eloop_idx);
        if (rv != EXB_OK) {
            exb_log_error(mod->exb_ref, "Failed to initialize lua instance for eloop_idx: %d\n", eloop_idx);
            return rv;
        }
    }

    struct exb_str path;

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
    struct lua_module *mod = (struct lua_module *) module;
    for (int i=0; i < MAX_CPU_COUNT; i++) {
        if (mod->lua_instances[i].L) {
            lua_close(mod->lua_instances[i].L);
            mod->lua_instances[i].L = NULL;
        }
    }
    exb_free(exb_ref, module);
}
int exb_lua_init(struct exb *exb_ref, struct exb_server *server, char *module_args, struct exb_http_server_module **module_out) {
    (void) module_args;
    struct lua_module *mod = exb_malloc(exb_ref, sizeof(struct lua_module));
    if (!mod)
        return EXB_NOMEM_ERR;
    memset(mod, 0, sizeof *mod);
    mod->exb_ref = exb_ref;
    mod->head.destroy = destroy_module;
    
    *module_out = (struct exb_http_server_module*)mod;
    return EXB_OK;
}
