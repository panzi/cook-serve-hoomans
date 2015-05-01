CC=gcc
XXD=xxd
BINEXT=
TARGET=$(shell uname|tr '[A-Z]' '[a-z]')$(shell getconf LONG_BIT)
COMMON_CFLAGS=-Wall -Werror -Wextra -std=gnu99 -O2
POSIX_CFLAGS=$(COMMON_CFLAGS) -pedantic
WINDOWS_CFLAGS=$(COMMON_CFLAGS)
BUILDDIR=build/$(TARGET)

ifeq ($(TARGET),win32)
	CC=i686-w64-mingw32-gcc
	CFLAGS=$(WINDOWS_CFLAGS) -m32
	LDFLAGS=-m32
	LIBS=$(WINDOWS_LIBS)
	BINEXT=.exe
else
ifeq ($(TARGET),win64)
	CC=x86_64-w64-mingw32-gcc
	CFLAGS=$(WINDOWS_CFLAGS) -m64
	LDFLAGS=-m64
	LIBS=$(WINDOWS_LIBS)
	BINEXT=.exe
else
ifeq ($(TARGET),linux32)
	CFLAGS=$(POSIX_CFLAGS) -m32
	LDFLAGS=-m32
else
ifeq ($(TARGET),linux64)
	CFLAGS=$(POSIX_CFLAGS) -m64
	LDFLAGS=-m64
endif
endif
endif
endif

.PHONY: all clean

all: $(BUILDDIR)/cook_serve_hoomans$(BINEXT)

hoomans_png.h: hoomans.png
	$(XXD) -i $< > $@

$(BUILDDIR)/cook_serve_hoomans$(BINEXT): $(BUILDDIR)/cook_serve_hoomans.o
	$(CC) $(LDFLAGS) $< -o $@

$(BUILDDIR)/cook_serve_hoomans.o: cook_serve_hoomans.c hoomans_png.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(BUILDDIR)/cook_serve_hoomans$(BINEXT) $(BUILDDIR)/cook_serve_hoomans.o hoomans_png.h
