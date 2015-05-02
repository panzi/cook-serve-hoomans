CC=gcc
XXD=xxd
BINEXT=
TARGET=$(shell uname|tr '[A-Z]' '[a-z]')$(shell getconf LONG_BIT)
COMMON_CFLAGS=-Wall -Werror -Wextra -std=gnu99 -O2
POSIX_CFLAGS=$(COMMON_CFLAGS) -pedantic
CFLAGS=$(COMMON_CFLAGS)
BUILDDIR=build/$(TARGET)
ARCH_FLAGS=

ifeq ($(TARGET),win32)
	CC=i686-w64-mingw32-gcc
	ARCH_FLAGS=-m32
	BINEXT=.exe
else
ifeq ($(TARGET),win64)
	CC=x86_64-w64-mingw32-gcc
	ARCH_FLAGS=-m64
	BINEXT=.exe
else
ifeq ($(TARGET),linux32)
	CFLAGS=$(POSIX_CFLAGS)
	ARCH_FLAGS=-m32
else
ifeq ($(TARGET),linux64)
	CFLAGS=$(POSIX_CFLAGS)
	ARCH_FLAGS=-m64
endif
endif
endif
endif

.PHONY: all clean

all: $(BUILDDIR)/cook_serve_hoomans$(BINEXT)

hoomans_png.h: hoomans.png
	$(XXD) -i $< > $@

$(BUILDDIR)/cook_serve_hoomans$(BINEXT): $(BUILDDIR)/cook_serve_hoomans.o
	$(CC) $(ARCH_FLAGS) $< -o $@

$(BUILDDIR)/cook_serve_hoomans.o: cook_serve_hoomans.c hoomans_png.h
	$(CC) $(ARCH_FLAGS) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(BUILDDIR)/cook_serve_hoomans$(BINEXT) $(BUILDDIR)/cook_serve_hoomans.o hoomans_png.h
