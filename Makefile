CC=gcc
POSTERIZE=posterize
CONVERT=convert
XXD=xxd
BINEXT=
TARGET=$(shell uname|tr '[A-Z]' '[a-z]')$(shell getconf LONG_BIT)
BUILDDIR=build/$(TARGET)
INCLUDE=-I$(BUILDDIR)
COMMON_CFLAGS=-Wall -Werror -Wextra -std=gnu11 -O2 $(INCLUDE)
POSIX_CFLAGS=$(COMMON_CFLAGS) -pedantic
CFLAGS=$(COMMON_CFLAGS)
STEAMDIR=~/.steam/steam
CSD_DIR=$(STEAMDIR)/SteamApps/common/CookServeDelicious
ARCH_FLAGS=

QP_OBJ=$(BUILDDIR)/patch_game.o \
       $(BUILDDIR)/quick_patch.o

CSH_OBJ=$(BUILDDIR)/patch_game.o \
        $(BUILDDIR)/cook_serve_hoomans.o \
        $(BUILDDIR)/hoomans_png.o \
        $(BUILDDIR)/icons_png.o

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

.PHONY: all clean cook_serve_hoomans quick_patch patch setup

all: cook_serve_hoomans quick_patch

cook_serve_hoomans: $(BUILDDIR)/cook_serve_hoomans$(BINEXT)

quick_patch: $(BUILDDIR)/quick_patch$(BINEXT)

setup:
	mkdir -p $(BUILDDIR)

patch: $(BUILDDIR)/cook_serve_hoomans$(BINEXT)
	$<

$(BUILDDIR)/%_png.c: %.png
	$(XXD) -i $< > $@

$(BUILDDIR)/%_png.h: %.png ./mkheader.sh
	./mkheader.sh $< $@

$(BUILDDIR)/%.o: %.c
	$(CC) $(ARCH_FLAGS) $(CFLAGS) -c $< -o $@

$(BUILDDIR)/%.o: $(BUILDDIR)/%.c
	$(CC) $(ARCH_FLAGS) $(CFLAGS) -c $< -o $@

$(BUILDDIR)/cook_serve_hoomans.o: cook_serve_hoomans.c $(BUILDDIR)/hoomans_png.h $(BUILDDIR)/icons_png.h
	$(CC) $(ARCH_FLAGS) $(CFLAGS) cook_serve_hoomans.c -o $@

$(BUILDDIR)/cook_serve_hoomans$(BINEXT): $(CSH_OBJ)
	$(CC) $(ARCH_FLAGS) $(CSH_OBJ) -o $@

$(BUILDDIR)/quick_patch$(BINEXT): $(QP_OBJ)
	$(CC) $(ARCH_FLAGS) $(QP_OBJ) -o $@

hoomans.png: opt/hoomans_opt.png
	$(CONVERT) $< \
		-define png:compression-filter=5 \
		-define png:compression-level=9 \
		-define png:compression-strategy=3 $@

opt/hoomans_opt.png: hoomans_post.png opt/Makefile
	$(MAKE) -C opt

opt/Makefile: opt/configure.sh
	cd opt; ./configure.sh

hoomans_post.png: hoomans_src.png
	$(POSTERIZE) 255 $< $@

clean: opt/Makefile
	$(MAKE) -C opt clean
	rm -f \
		$(BUILDDIR)/hoomans_png.h \
		$(BUILDDIR)/hoomans_png.c \
		$(BUILDDIR)/icons_png.h \
		$(BUILDDIR)/icons_png.c \
		$(BUILDDIR)/patch_game.o \
		$(BUILDDIR)/cook_serve_hoomans.o \
		$(BUILDDIR)/quick_patch.o \
		$(BUILDDIR)/hoomans_png.o \
		$(BUILDDIR)/icons_png.o \
		$(BUILDDIR)/cook_serve_hoomans$(BINEXT) \
		$(BUILDDIR)/quick_patch$(BINEXT) \
		hoomans_post.png \
		hoomans.png
