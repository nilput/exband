CFLAGS := -Wall -fPIC -Wno-unused-function
EXB_LIB_DIR := ../../../../
LDFLAGS := -L $(EXB_LIB_DIR) -Wl,-rpath=\$$ORIGIN/ 
LDLIBS := -lexb
debug: CFLAGS += -O0 -g3
debug: all
release: CFLAGS += -O2 -march=native -mtune=native -DEXB_NO_ASSERTS
release: all
san: CFLAGS += -fsanitizer=address -g3 -O0
san: all

all: exb_perf.so
exb_perf.so: exb_perf.c exb_event_perf_act.c
	$(CC)  $(CFLAGS) -shared  $^ $(LDFLAGS) $(LDLIBS) -o $@
