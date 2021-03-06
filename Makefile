LDLIBS = -pthread -ldl
CFLAGS = -I othersrc/ -I third_party/ -fPIC -Wall -Wno-unused-but-set-variable \
											-Wno-unused-function -Wno-unused-label \
											-Wno-unused-variable -Wno-unused-but-set-variable
EXB_SRC = src/exb/exb_utils.c \
			src/exb/exb_thread.c \
			src/exb/http/http_server.c \
			src/exb/http/http_server_events.c \
			src/exb/http/http_request.c \
			src/exb/http/http_response.c \
			src/exb/exb_pcontrol.c \
			src/exb/exb_main_config.c \
			src/exb/http/http_server_module.c \
			src/exb/http/handlers/exb_fileserv.c \
			src/exb/http/listener/http_server_listener_select.c \
			src/exb/http/listener/http_server_listener_epoll.c

LDFLAGS := -Wl,-rpath=\$$ORIGIN/:\$$ORIGIN/obj/
EXB_OBJ = $(patsubst %.c,%.o, $(patsubst src/%,obj/%, $(EXB_SRC)))
EXB_LINK = 
OPTIONAL = 
EXB_LINK_ARCHIVES = 
DEP := $(EXB_OBJ:.o=.d)

ifneq ($(or $(WITH_SSL), 0), 0)
OPTIONAL += mod_ssl
CFLAGS += -DEXB_WITH_SSL
EXB_LINK_ARCHIVES += -Wl,--whole-archive obj/libexb_mod_ssl.a -Wl,--no-whole-archive
LDLIBS += -lcrypto -lssl
endif

all: fast-release example-basic-module
main_targets: $(OPTIONAL) exb exb_static obj/libexb.so

-include $(DEP)

test:
	./tests/run_all

debug: CFLAGS += -g3 -O0 -DTRACK_RQSTATE_EVENTS -DEXB_DEBUG
debug: main_targets

trace: CFLAGS += -Wl,-export-dynamic -ldl -DEXB_TRACE -DEXB_DEBUG -g3 -O0 -finstrument-functions -Wall -Wno-unused-function -Wno-unused-label -Wno-unused-variable 
trace: main_targets
debug-no-assert: CFLAGS += -DEXB_DEBUG -g3 -O0 -Wall -Wno-unused-function -Wno-unused-label -Wno-unused-variable -DEXB_NO_ASSERTS -fno-inline-small-functions
debug-no-assert: main_targets
release: CFLAGS += -g -O2 -Wall -Wno-unused-function
release: main_targets
fast-release: CFLAGS += -flto -mtune=native -march=native -O2 -Wall -Wno-unused-function -DEXB_NO_ASSERTS -DEXB_NO_LOGGIN
fast-release: main_targets
no-release: CFLAGS += -g -O1 -Wall -Wno-unused-function -DEXB_NO_ASSERTS
no-release: main_targets
san: CFLAGS += -DEXB_DEBUG -g3 -O0 -Wall -Wno-unused-function -fsanitize=address
san: main_targets
san-thread: CFLAGS += -DEXB_DEBUG -g3 -O0 -Wall -Wno-unused-function -fsanitize=thread
san-thread: main_targets
profile: CFLAGS += -DENABLE_DBGPERF -g3 -O3 -Wall -Wno-unused-function -DEXB_NO_ASSERTS
profile: EXB_SRC += othersrc/dbgperf/dbgperf.c
profile: obj/libexb.so

include src/exb/mods/ssl/module.mk
mod_ssl: obj/libexb_mod_ssl.a

example-basic-module:
	for d in basic; do \
		(cd "examples/$$d" && $(MAKE) -B) || exit 1; \
	done

obj/%.o: src/%.c
	@mkdir -p '$(@D)'
	$(CC) -c -MMD $(CFLAGS) -o $@ $<

obj/libexb.so: $(EXB_OBJ)
	$(CC)  -shared -o $@ $(CFLAGS) $(LDFLAGS) -L ./obj $(EXB_OBJ) $(EXB_LINK_ARCHIVES) $(LDLIBS)
exb: src/exb/exb_main.c obj/libexb.so
	$(CC)  -o $@ $(CFLAGS) $(LDFLAGS) src/exb/exb_main.c -L ./obj  -lexb $(LDLIBS)
exb_static: src/exb/exb_main.c $(EXB_OBJ)
	$(CC)  -o $@ $(CFLAGS) $(LDFLAGS) src/exb/exb_main.c $(EXB_OBJ) -L ./obj -lexb $(EXB_LINK_ARCHIVES) $(LDLIBS)

clean: 
	@rm -f exb exb_static perf.data* 2>/dev/null || true
	@rm -rf obj oprofile_data
	@find examples/ -name '*.so' -exec 'rm' '{}' ';'
.PHONY: clean all main_targets example-basic-module test
