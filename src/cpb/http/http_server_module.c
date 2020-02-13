#include <dlfcn.h>
#include <string.h>
#include "../cpb.h"
#include "../cpb_str.h"
#include "http_server_module.h"
#include "http_server_module_internal.h"
/*name := "dll_name:func_name"*/
int cpb_http_server_module_load(struct cpb *cpb_ref, struct cpb_server *server, char *handler_name, char *module_args, struct cpb_http_server_module **module_out, void **handle_out) {
    
    char *seperator = strchr(handler_name, ':');
    if (!seperator)
        return CPB_INVALID_ARG_ERR;
    struct cpb_str dll_name;
    struct cpb_str func_name;
    int err = cpb_str_init_strlcpy(cpb_ref, &dll_name, handler_name, seperator-handler_name);
    if (err != CPB_OK)
        return err;
    err = cpb_str_init_strcpy(cpb_ref, &func_name, seperator+1);
    if (err != CPB_OK) {
        cpb_str_deinit(cpb_ref, &dll_name);
        return err;
    }
    if ((err = cpb_str_strappend(cpb_ref, &func_name, "_init")) != CPB_OK) {
        cpb_str_deinit(cpb_ref, &dll_name);
        cpb_str_deinit(cpb_ref, &func_name);
        return err;
    }
    void *handle = dlopen(dll_name.str, RTLD_LAZY | RTLD_GLOBAL);
    if (!handle) {
        char *e = dlerror();
        fprintf(stderr, "Failed to load lib: \"%s\": %s\n", dll_name.str, e ? e : "");
        cpb_str_deinit(cpb_ref, &dll_name);
        cpb_str_deinit(cpb_ref, &func_name);
        return CPB_NOT_FOUND;
    }
    cpb_http_server_module_init_func init_func = dlsym(handle, func_name.str);
    int init_success = init_func != NULL && (init_func(cpb_ref, server, module_args, module_out) == 0);
    if (init_success) {
        *handle_out = handle;
    }
    else {
        dlclose(handle);
    }
    cpb_str_deinit(cpb_ref, &dll_name);
    cpb_str_deinit(cpb_ref, &func_name);
    if (!init_func)
        return CPB_NOT_FOUND;
    else if (!init_success)
        return CPB_INIT_ERROR;
    return CPB_OK;
}
int cpb_http_server_module_unload(struct cpb *cpb_ref, struct cpb_http_server_module *module, void *handle) {
    
    if (module->destroy)
        module->destroy(module, cpb_ref);
    dlclose(handle);
    return CPB_OK;
}
