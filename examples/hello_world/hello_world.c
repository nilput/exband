#include "cpb/cpb.h"
#include "cpb/http/http_server_module.h"
#include "cpb/http/http_request.h"

struct hello_world_module {
    struct cpb_http_server_module head;
    struct cpb *cpb_ref;
    int count;
};
static int handle_request(struct cpb_http_server_module *module, struct cpb_request_state *rqstate, int reason) {
    struct hello_world_module *mod = (struct hello_world_module *) module;

    struct cpb_str path;
    //cpb_request_repr(rqstate);

    cpb_str_init(mod->cpb_ref, &path);
    cpb_str_slice_to_copied_str(mod->cpb_ref, rqstate->path_s, rqstate->input_buffer, &path);
    if (cpb_str_streqc(mod->cpb_ref, &path, "/post") || cpb_str_streqc(mod->cpb_ref, &path, "/post/")) {
        struct cpb_str key,value;
        cpb_str_init_const_str(&key, "Content-Type");
        cpb_str_init_const_str(&value, "text/html");
        cpb_response_append_body_cstr(rqstate, "<!DOCTYPE html>");
        cpb_response_append_body_cstr(rqstate, "<html>"
                                                      "<head>"
                                                      "    <title>CPBIN!</title>"
                                                      "</head>"
                                                      "<body>");
        if (cpb_request_has_body(rqstate)) {
            if (reason == CPB_HTTP_HANDLER_HEADERS) {
                rqstate->body_handling = CPB_HTTP_B_BUFFER;
            }
            else {
                cpb_response_append_body_cstr(rqstate, "You posted: <br>");
                cpb_response_append_body_cstr(rqstate, "<p>");

                /*XSS!*/
                if (rqstate->body_decoded.str)
                    cpb_response_append_body_cstr(rqstate, rqstate->body_decoded.str);

                cpb_response_append_body_cstr(rqstate, "</p>");
                cpb_response_append_body_cstr(rqstate, "</body>"
                                                              "</html>");
                cpb_response_end(rqstate);
            }
        }
        else {
            cpb_response_set_header(rqstate, &key, &value);
            cpb_response_append_body_cstr(rqstate, "<p> Post something! </p> <br>");
            cpb_response_append_body_cstr(rqstate, "<form action=\"\" method=\"POST\">"
                                                          "Text: <input type=\"text\" name=\"text\"><br>"
                                                          "</form>");
            cpb_response_append_body_cstr(rqstate, "</body>"
                                                          "</html>");
            cpb_response_end(rqstate);
        }
        
    }
    else if (cpb_str_startswithc(mod->cpb_ref, &path, "/count")) 
    {
        struct cpb_str key,value, tmp;
        cpb_str_init_const_str(&key, "Content-Type");
        cpb_str_init_const_str(&value, "text/html");
        cpb_str_init(mod->cpb_ref, &tmp);
        cpb_sprintf(mod->cpb_ref, &tmp, "<!DOCTYPE html>"
                                        "<html>"
                                        "<head>"
                                        "    <title>CPBIN!</title>"
                                        "</head>"
                                        "<body>"
                                        "<p>Count : <b>%d</b></p><br>"
                                        "<form action=\"/reset/\"><button>reset</button></form>"
                                        "</body>"
                                        "</html>", mod->count++);
        cpb_response_append_body_cstr(rqstate, tmp.str);
        cpb_str_deinit(mod->cpb_ref, &tmp);
        cpb_response_end(rqstate);
    }
    else if (cpb_str_streqc(mod->cpb_ref, &path, "/reset") || cpb_str_startswithc(mod->cpb_ref, &path, "/reset/")) {
        mod->count = 0;
        cpb_response_redirect_and_end(rqstate, 307, "/count/");
    }
    else {
        struct cpb_str key,value;
        cpb_str_init_const_str(&key, "Content-Type");
        cpb_str_init_const_str(&value, "text/plain");
        cpb_response_set_header(rqstate, &key, &value);
        cpb_response_append_body(rqstate, "Hello World!\r\n", 14);
        struct cpb_str str;
        
        cpb_str_init(mod->cpb_ref, &str);
        
        cpb_sprintf(mod->cpb_ref, &str, "Requested URL: '%s'", path.str);
        
        cpb_response_append_body(rqstate, str.str, str.len);
        cpb_str_deinit(mod->cpb_ref, &str);
        cpb_response_end(rqstate);
    }
    
    cpb_str_deinit(mod->cpb_ref, &path);   
}
static void destroy_module(struct cpb_http_server_module *module, struct cpb *cpb) {
    cpb_free(cpb, module);
}
int handler_init(struct cpb *cpb, struct cpb_server *server, char *module_args, struct cpb_http_server_module **module_out) {
    (void) module_args;
    struct hello_world_module *mod = cpb_malloc(cpb, sizeof(struct hello_world_module));
    if (!mod)
        return CPB_NOMEM_ERR;
    mod->cpb_ref = cpb;
    
    mod->head.destroy = destroy_module;
    mod->count = 0;
    
    if (cpb_server_set_module_request_handler(server, (struct cpb_http_server_module*)mod, handle_request) != CPB_OK) {
        destroy_module((struct cpb_http_server_module*)mod, cpb);
        return CPB_MODULE_LOAD_ERROR;
    }
    *module_out = (struct cpb_http_server_module*)mod;
    return CPB_OK;
}
