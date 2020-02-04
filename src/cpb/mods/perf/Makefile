CFLAGS := -Wall -fPIC -Wno-unused-function
CPB_LIB_DIR := ../../../../
LDFLAGS := -L $(CPB_LIB_DIR) -Wl,-rpath=\$$ORIGIN/ 
LDLIBS := -lcpb
debug: CFLAGS += -O0 -g3
debug: all
release: CFLAGS += -O2 -march=native -mtune=native -DCPB_NO_ASSERTS
release: all
san: CFLAGS += -fsanitizer=address -g3 -O0
san: all

all: cpb_perf.so
cpb_perf.so: cpb_perf.c cpb_event_perf_act.c
	$(CC)  $(CFLAGS) -shared  $^ $(LDFLAGS) $(LDLIBS) -o $@
