debug: CFLAGS += -g3 -O0 -Wall -Wno-unused-function
debug: all
all : server_main
server_main: server_main.c  cpb_utils.c http/http_server.c http/http_server_events.c http/http_request.c http/http_response.c
