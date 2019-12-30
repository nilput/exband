debug: CFLAGS += -g3 -O0 -Wall
debug: all
all : server_main
server_main: server_main.c server.c server_events.c
