CFLAGS := -Wall -fPIC -Wno-unused-function $(shell python3-config --cflags)
CPB_LIB_DIR := ../../../../
#LDFLAGS := -L $(CPB_LIB_DIR) -Wl,-rpath=\$$ORIGIN/ $(shell python3-config --ldflags)
LDFLAGS := -L $(CPB_LIB_DIR) $(shell python3-config --ldflags) -L. -Wl,-rpath=\$$ORIGIN/
LDLIBS :=  $(shell python3-config --libs) -lcpb -lcpb_ffi
PYTHON3 := python3
debug: CFLAGS += -O0 -g3
debug: all
release: CFLAGS += -O2 -march=native -mtune=native -DCPB_NO_ASSERTS
release: all
san: CFLAGS += -fsanitizer=address -g3 -O0
san: all
.PHONY: all clean

all: cpb_py.so _cpb_py/_cpb_ffi.so
libcpb_py.so: src/mod_py.c libcpb_ffi.so
	$(CC)  $(CFLAGS) -shared  $^ -o $@  $(LDLIBS) $(LDFLAGS)
cpb_py.so: libcpb_py.so
	rm cpb_py.so 2>/dev/null || true
	ln -s $^ $@
libcpb_ffi.so:
	rm _cpb_py/*.so build/*.so 2>/dev/null || true
	(cd build && $(PYTHON3) ../src/ffi_build.py )
	cp build/*.so libcpb_ffi.so
_cpb_py/_cpb_ffi.so: libcpb_ffi.so 
	cd _cpb_py/ && ln -s ../libcpb_ffi.so _cpb_ffi.so
clean:
	rm -f libcpb_py.so cpb_py.so build/_cpb_ffi.o build/_cpb_ffi.c build/*.so  2>/dev/null || true
