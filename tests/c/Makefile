LDLIBS := -lcmocka -L ../../obj -lexb 
LDFLAGS := -L. -Wl,-rpath=\$$ORIGIN/../../obj
CFLAGS := -Wall -Wno-pointer-sign
debug: CFLAGS += -g3 -O0
debug: all
TARGETS := test_decode test_recycle test_path_resolve
test_decode_data.h: 
	/usr/bin/env python3 generators/gen_test_decode_data.py
test_decode: test_decode.c test_decode_data.h
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) $(LDLIBS)
all: $(TARGETS)
clean: 
	rm -f $(TARGETS)
.PHONY: all clean
