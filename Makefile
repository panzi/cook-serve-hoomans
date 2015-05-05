CC=gcc
XXD=xxd
BINEXT=
TARGET=$(shell uname|tr '[A-Z]' '[a-z]')$(shell getconf LONG_BIT)
COMMON_CFLAGS=-Wall -Werror -Wextra -std=gnu99 -O2
POSIX_CFLAGS=$(COMMON_CFLAGS) -pedantic
CFLAGS=$(COMMON_CFLAGS)
BUILDDIR=build/$(TARGET)
OBJ=$(BUILDDIR)/cook_serve_hoomans.o $(BUILDDIR)/hoomans_png.o $(BUILDDIR)/icons_png.o
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

$(BUILDDIR)/%_png.c: %.png
	$(XXD) -i $< > $@

$(BUILDDIR)/%.o: %_png.c
	$(CC) $(ARCH_FLAGS) $(CFLAGS) -c $< -o $@

$(BUILDDIR)/cook_serve_hoomans.o: cook_serve_hoomans.c
	$(CC) $(ARCH_FLAGS) $(CFLAGS) -c $< -o $@

$(BUILDDIR)/cook_serve_hoomans$(BINEXT): $(OBJ)
	$(CC) $(ARCH_FLAGS) $(OBJ) -o $@

clean:
	rm -f \
		$(BUILDDIR)/hoomans_png.c \
		$(BUILDDIR)/icons_png.c \
		$(OBJ) \
		$(BUILDDIR)/cook_serve_hoomans$(BINEXT)
