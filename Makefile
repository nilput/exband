.PHONY: clean all
CFLAGS := -I othersrc/ -fPIC
LDLIBS := -pthread -ldl
SERVER_MAIN_DEPS := src/cpb_utils.c src/http/http_server.c
SERVER_MAIN_DEPS := $(SERVER_MAIN_DEPS) src/http/http_server_events.c src/http/http_request.c
SERVER_MAIN_DEPS := $(SERVER_MAIN_DEPS) src/http/http_response.c src/http/http_server_listener_select.c
SERVER_MAIN_DEPS := $(SERVER_MAIN_DEPS) src/http/http_server_listener_epoll.c src/http/http_handler_module.c

debug: CFLAGS += -DCPB_DEBUG -g3 -O0 -Wall -Wno-unused-function
debug: all
release: CFLAGS += -g3 -O3 -Wall -Wno-unused-function
release: all
san: CFLAGS += -DCPB_DEBUG -g3 -O0 -Wall -Wno-unused-function -fsanitize=address
san: all


profile: CFLAGS += -DENABLE_DBGPERF -g3 -O3 -Wall -Wno-unused-function
profile: SERVER_MAIN_DEPS += othersrc/dbgperf/dbgperf.c
profile: server_main



libcpb.so: $(SERVER_MAIN_DEPS)
	$(CC)  -shared $(CFLAGS) $(LDFLAGS) -o $@  $^ $(LDLIBS)
server_main: $(SERVER_MAIN_DEPS) libcpb.so
	$(CC)  -o $@ $(CFLAGS) $(LDFLAGS) src/server_main.c  $(LDLIBS) -L. -Wl,-rpath=\$$ORIGIN/ -lcpb

all : server_main libcpb.so
server_main: $(SERVER_MAIN_DEPS)
clean: 
	@rm -f server_main libcpb.so 2>/dev/null
