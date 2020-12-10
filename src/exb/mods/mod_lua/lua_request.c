#include "lua_request.h"
#include <exb/http/http_request.h>

/*exb_response_append(rqstate, data)*/
int exb_response_append_body_l(lua_State *L) {
    char *chunk = "";
    size_t chunk_len = 0;
    
    if (!lua_islightuserdata(L, 1) || !lua_isstring(L, 2)) {
        return 0;
    }
    struct exb_request_state *rqstate = lua_touserdata(L, 1);
    chunk = lua_tolstring(L, 2, &chunk_len);
    //TODO: handle return value
    exb_response_append_body(rqstate, chunk, chunk_len);
    return 0;
}
int exb_response_end_l(lua_State *L) {
    if (!lua_islightuserdata(L, 1)) {
        return 0;
    }
    struct exb_request_state *rqstate = lua_touserdata(L, 1);
    //TODO: handle return value
    exb_response_end(rqstate);
    return 0;
}
int exb_request_get_path_l(lua_State *L) {
    if (!lua_islightuserdata(L, 1)) {
        return 0;
    }
    struct exb_request_state *rqstate = lua_touserdata(L, 1);
    struct exb *exb_ref = exb_request_get_exb_ref(rqstate);
    struct exb_str path;
    if (exb_request_get_path_copy(rqstate, &path) != EXB_OK) {
        lua_pushnil(L);
    }
    else {
        lua_pushlstring(L, path.str, path.len);
        exb_str_deinit(exb_ref, &path);
    }
    return 1;
}


int exb_lua_request_functions_init(lua_State *L) {
    lua_pushcfunction(L, exb_response_end_l);
    lua_setglobal(L, "exb_response_end");
    lua_pushcfunction(L, exb_response_append_body_l);
    lua_setglobal(L, "exb_response_append_body");
    lua_pushcfunction(L, exb_request_get_path_l);
    lua_setglobal(L, "exb_request_get_path");
    return EXB_OK;
}