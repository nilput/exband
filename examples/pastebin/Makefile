EXB_LIB_DIR := ../../
EXB_INCLUDE_DIR := ../../src/
CFLAGS := -I ../../othersrc/ -fPIC -I $(EXB_INCLUDE_DIR)
LDLIBS := -L $(EXB_LIB_DIR) -lexb -lsqlite3

.PHONY: clean all
debug: CFLAGS += -g3 -O0 -Wall -Wno-unused-function
debug: all
release: CFLAGS += -g3 -O3 -Wall -Wno-unused-function
release: all

pastebin.so: pastebin.c
	$(CC)  -shared $(CFLAGS) $(LDFLAGS) -o $@  $^ $(LDLIBS)

all: pastebin.so
clean: 
	@rm -f pastebin.so 2>/dev/null
