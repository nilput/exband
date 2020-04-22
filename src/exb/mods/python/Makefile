CFLAGS := -Wall -fPIC -Wno-unused-function $(shell python3-config --cflags)
EXB_LIB_DIR := ../../../../
#LDFLAGS := -L $(EXB_LIB_DIR) -Wl,-rpath=\$$ORIGIN/ $(shell python3-config --ldflags)
LDFLAGS := -L $(EXB_LIB_DIR) $(shell python3-config --ldflags) -L. -Wl,-rpath=\$$ORIGIN/
LDLIBS :=  $(shell python3-config --libs) -lexb -lexb_ffi
PYTHON3 := python3
debug: CFLAGS += -O0 -g3
debug: all
release: CFLAGS += -O2 -march=native -mtune=native -DEXB_NO_ASSERTS
release: all
san: CFLAGS += -fsanitizer=address -g3 -O0
san: all
.PHONY: all clean

all: exb_py.so _exb_py/_exb_ffi.so
libexb_py.so: src/mod_py.c libexb_ffi.so
	$(CC)  $(CFLAGS) -shared  $^ -o $@  $(LDLIBS) $(LDFLAGS)
exb_py.so: libexb_py.so
	rm exb_py.so 2>/dev/null || true
	ln -s $^ $@
libexb_ffi.so:
	rm _exb_py/*.so build/*.so 2>/dev/null || true
	(cd build && $(PYTHON3) ../src/ffi_build.py )
	cp build/*.so libexb_ffi.so
_exb_py/_exb_ffi.so: libexb_ffi.so 
	cd _exb_py/ && ln -s ../libexb_ffi.so _exb_ffi.so
clean:
	rm -f libexb_py.so exb_py.so build/_exb_ffi.o build/_exb_ffi.c build/*.so  2>/dev/null || true
