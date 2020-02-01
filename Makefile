.PHONY: clean all
CFLAGS := -I othersrc/ -fPIC
LDLIBS := -pthread -ldl
SERVER_MAIN_DEPS := src/cpb/cpb_utils.c src/cpb/http/http_server.c
SERVER_MAIN_DEPS := $(SERVER_MAIN_DEPS) src/cpb/http/http_server_events.c src/cpb/http/http_request.c
SERVER_MAIN_DEPS := $(SERVER_MAIN_DEPS) src/cpb/http/http_response.c src/cpb/http/http_server_listener_select.c
SERVER_MAIN_DEPS := $(SERVER_MAIN_DEPS) src/cpb/http/http_server_listener_epoll.c src/cpb/http/http_server_module.c
SERVER_MAIN_DEPS := $(SERVER_MAIN_DEPS) src/cpb/cpb_threadpool.c

debug: CFLAGS += -DCPB_DEBUG -g3 -O0 -Wall -Wno-unused-function -Wno-unused-label -Wno-unused-variable
debug: all
release: CFLAGS += -g -O2 -Wall -Wno-unused-function 
release: all
fast-release: CFLAGS += -flto -g -O2 -Wall -Wno-unused-function -DCPB_NO_ASSERTS
fast-release: all
no-release: CFLAGS += -g -O1 -Wall -Wno-unused-function -DCPB_NO_ASSERTS
no-release: all
san: CFLAGS += -DCPB_DEBUG -g3 -O0 -Wall -Wno-unused-function -fsanitize=address
san: all


profile: CFLAGS += -DENABLE_DBGPERF -g3 -O3 -Wall -Wno-unused-function -DCPB_NO_ASSERTS
profile: SERVER_MAIN_DEPS += othersrc/dbgperf/dbgperf.c
profile: server_main libcpb.so



libcpb.so: $(SERVER_MAIN_DEPS)
	$(CC)  -shared $(CFLAGS) $(LDFLAGS) -o $@  $(SERVER_MAIN_DEPS) $(LDLIBS)
server_main: $(SERVER_MAIN_DEPS) libcpb.so
	$(CC)  -o $@ $(CFLAGS) $(LDFLAGS) src/cpb/server_main.c  $(LDLIBS) -L. -Wl,-rpath=\$$ORIGIN/ -lcpb

all : server_main libcpb.so
server_main: $(SERVER_MAIN_DEPS)
clean: 
	@rm -f server_main libcpb.so 2>/dev/null
