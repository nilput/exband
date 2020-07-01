LDLIBS := -pthread -ldl
CFLAGS = -I othersrc/ -I third_party/ -fPIC 
EXB_SRC = src/exb/exb_utils.c src/exb/http/http_server.c \
                     src/exb/http/http_server_events.c src/exb/http/http_request.c \
                     src/exb/http/http_response.c src/exb/http/http_server_listener_select.c \
                     src/exb/http/http_server_listener_epoll.c src/exb/http/http_server_module.c \
                     src/exb/http/exb_fileserv.c \
                     src/exb/exb_threadpool.c src/exb/exb_pcontrol.c \
                     src/exb/exb_main_config.c
EXB_OBJ = $(patsubst %.c,%.o, $(patsubst src/%,obj/%, $(EXB_SRC)))
EXB_LINK = 
OPTIONAL = 
DEP := $(EXB_OBJ:.o=.d)



ifneq ($(or $(WITH_SSL), 0), 0)
OPTIONAL += mod_ssl
CFLAGS += -DEXB_WITH_SSL
endif

debug:
all: exb $(OPTIONAL) obj/libexb.so

-include $(DEP)

debug: CFLAGS += -DEXB_DEBUG -g3 -O0 -Wall -Wno-unused-but-set-variable -Wno-unused-function -Wno-unused-label -Wno-unused-variable #-DTRACK_RQSTATE_EVENTS
debug: all

trace: CFLAGS += -Wl,-export-dynamic -ldl -DEXB_TRACE -DEXB_DEBUG -g3 -O0 -finstrument-functions -Wall -Wno-unused-function -Wno-unused-label -Wno-unused-variable 
trace: all
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
profile: EXB_SRC += othersrc/dbgperf/dbgperf.c
profile: server_main obj/libexb.s

include src/exb/mods/ssl/module.mk
mod_ssl: obj/libexb_mod_ssl.a
mod_ssl: EXB_LINK_ARCHIVES += -Wl,--whole-archive obj/libexb_mod_ssl.a -Wl,--no-whole-archive

examples:
	for d in basic hello_world pastebin; do \
		(cd "examples/$$d" && $(MAKE) -B) || exit 1; \
	done



obj/%.o: src/%.c
	@mkdir -p '$(@D)'
	$(CC) -c -MMD $(CFLAGS) -o $@ $<

obj/libexb.so: $(EXB_OBJ)
	$(CC)  -shared -o $@ $(CFLAGS) $(LDFLAGS) -L ./obj $(EXB_LINK_ARCHIVES) $(LDLIBS) $(EXB_OBJ)
exb: src/exb/exb_main.c obj/libexb.so
	$(CC)  -o $@ $(CFLAGS) $(LDFLAGS) src/exb/exb_main.c -L ./obj -Wl,-rpath=\$$ORIGIN/:\$$ORIGIN/obj/ -lexb $(LDLIBS)

clean: 
	@rm -f exb perf.data* 2>/dev/null || true
	@rm -rf obj oprofile_data
.PHONY: clean all examples
