CFLAGS := -I othersrc/
SERVER_MAIN_DEPS := src/server_main.c  src/cpb_utils.c src/http/http_server.c src/http/http_server_events.c src/http/http_request.c src/http/http_response.c

debug: CFLAGS += -g3 -O0 -Wall -Wno-unused-function
debug: all
release: CFLAGS += -g3 -O3 -Wall -Wno-unused-function
release: all


profile: CFLAGS += -DENABLE_DBGPERF -g3 -O3 -Wall -Wno-unused-function
profile: SERVER_MAIN_DEPS += othersrc/dbgperf/dbgperf.c
profile: server_main



server_main: $(SERVER_MAIN_DEPS)
	$(CC) -o $(LD_FLAGS) $@ $(CFLAGS) $(SERVER_MAIN_DEPS)

all : server_main
server_main: $(SERVER_MAIN_DEPS)
