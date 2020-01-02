debug: CFLAGS += -g3 -O0 -Wall -Wno-unused-function
debug: all
all : server_main
server_main: server_main.c server.c server_events.c utils.c http_request.c http_response.c
