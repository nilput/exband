print("Lua works!")

function exb_handle_request(request_state)
    
    exb_response_append_body(request_state, "From Lua!")
    
    
    exb_response_end(request_state)
end