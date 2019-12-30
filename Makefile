debug: CFLAGS += -g3 -O0
debug: all
all : server_main
server_main: server_main.c server.c
