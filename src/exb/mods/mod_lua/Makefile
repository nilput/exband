TARGETS = mod_lua.so
EXB_LIB_DIR := ../../../../obj/
EXB_INCLUDE_DIR := ../../../

CFLAGS := -Wall -fPIC -I $(EXB_INCLUDE_DIR) $(shell pkg-config --cflags lua53) -Wno-unused-function -Wno-unused-variable
LDLIBS := -L $(EXB_LIB_DIR) -lexb $(shell pkg-config --libs lua53)

SRC := mod_lua.c

all: $(TARGETS)
mod_lua.so: mod_lua.c
	$(CC) -shared $(CFLAGS) $(LDFLAGS) -o $@ -Wl,-rpath=\$$ORIGIN/:\$$ORIGIN/obj/ $^ $(LDLIBS)

clean:
	rm -rf $(TARGETS) 2>/dev/null
