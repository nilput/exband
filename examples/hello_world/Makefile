CPB_LIB_DIR := ../../
CPB_INCLUDE_DIR := ../../src/
CFLAGS := -I ../../othersrc/ -fPIC -I $(CPB_INCLUDE_DIR)
LDLIBS := -L $(CPB_LIB_DIR) -lcpb

.PHONY: clean all
debug: CFLAGS += -g3 -O0 -Wall
debug: all
release: CFLAGS += -g3 -O3 -Wall -Wno-unused-function
release: all

hello_world.so: hello_world.c
	$(CC)  -shared $(CFLAGS) $(LDFLAGS) -o $@  $^ $(LDLIBS)

all: hello_world.so
clean: 
	@rm -f hello_world.so 2>/dev/null