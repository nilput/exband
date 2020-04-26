.PHONY: clean all
CFLAGS := -I othersrc/ -I third_party/ -fPIC 
LDLIBS := -pthread -ldl
SERVER_MAIN_DEPS := src/exb/exb_utils.c src/exb/http/http_server.c
SERVER_MAIN_DEPS := $(SERVER_MAIN_DEPS) src/exb/http/http_server_events.c src/exb/http/http_request.c
SERVER_MAIN_DEPS := $(SERVER_MAIN_DEPS) src/exb/http/http_response.c src/exb/http/http_server_listener_select.c
SERVER_MAIN_DEPS := $(SERVER_MAIN_DEPS) src/exb/http/http_server_listener_epoll.c src/exb/http/http_server_module.c
SERVER_MAIN_DEPS := $(SERVER_MAIN_DEPS) src/exb/exb_threadpool.c src/exb/exb_pcontrol.c

debug: CFLAGS += -DEXB_DEBUG -g3 -O0 -Wall -Wno-unused-function -Wno-unused-label -Wno-unused-variable #-DTRACK_RQSTATE_EVENTS
debug: all
debug-no-assert: CFLAGS += -DEXB_DEBUG -g3 -O0 -Wall -Wno-unused-function -Wno-unused-label -Wno-unused-variable -DEXB_NO_ASSERTS -fno-inline-small-functions
debug-no-assert: all
release: CFLAGS += -g -O2 -Wall -Wno-unused-function 
release: all
fast-release: CFLAGS += -flto -mtune=native -march=native -O2 -Wall -Wno-unused-function -DEXB_NO_ASSERTS
fast-release: all
no-release: CFLAGS += -g -O1 -Wall -Wno-unused-function -DEXB_NO_ASSERTS
no-release: all
san: CFLAGS += -DEXB_DEBUG -g3 -O0 -Wall -Wno-unused-function -fsanitize=address
san: all
san-thread: CFLAGS += -DEXB_DEBUG -g3 -O0 -Wall -Wno-unused-function -fsanitize=thread
san-thread: al
profile: CFLAGS += -DENABLE_DBGPERF -g3 -O3 -Wall -Wno-unused-function -DEXB_NO_ASSERTS
profile: SERVER_MAIN_DEPS += othersrc/dbgperf/dbgperf.c
profile: server_main libexb.s

libexb.so: $(SERVER_MAIN_DEPS)
	$(CC)  -shared $(CFLAGS) $(LDFLAGS) -o $@  $(SERVER_MAIN_DEPS) $(LDLIBS)
server_main: src/exb/server_main.c $(SERVER_MAIN_DEPS) libexb.so
	$(CC)  -o $@ $(CFLAGS) $(LDFLAGS) src/exb/server_main.c  $(LDLIBS) -L. -Wl,-rpath=\$$ORIGIN/ -lexb

all : server_main libexb.so
server_main: $(SERVER_MAIN_DEPS)
clean: 
	@rm -f server_main libexb.so oprofile_data perf.data* 2>/dev/null || true
