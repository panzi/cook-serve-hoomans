CC=gcc
BINEXT=
TARGET=$(shell uname|tr '[A-Z]' '[a-z]')$(shell getconf LONG_BIT)
BUILDDIR=build
BUILDDIR_BIN=$(BUILDDIR)/$(TARGET)
BUILDDIR_SRC=$(BUILDDIR)/src
INCLUDE=-I$(BUILDDIR_SRC)
COMMON_CFLAGS=-Wall -Werror -Wextra -std=gnu11 $(INCLUDE)
ifeq ($(DEBUG),ON)
	COMMON_CFLAGS+=-g -DDEBUG
else
	COMMON_CFLAGS+=-O2
endif
POSIX_CFLAGS=$(COMMON_CFLAGS) -pedantic -Wno-gnu-zero-variadic-macro-arguments -fdiagnostics-color
CFLAGS=$(COMMON_CFLAGS)
ARCH_FLAGS=

QP_OBJ=$(BUILDDIR_BIN)/quick_patch.o \
       $(BUILDDIR_BIN)/game_maker.o \
       $(BUILDDIR_BIN)/png_info.o

CSH_OBJ=$(BUILDDIR_BIN)/cook_serve_hoomans.o \
        $(BUILDDIR_BIN)/game_maker.o \
        $(BUILDDIR_BIN)/png_info.o \
        $(BUILDDIR_BIN)/hoomans_png.o \
        $(BUILDDIR_BIN)/catering_png.o \
        $(BUILDDIR_BIN)/icons_png.o

DMP_OBJ=$(BUILDDIR_BIN)/gmdump.o \
        $(BUILDDIR_BIN)/game_maker.o \
        $(BUILDDIR_BIN)/png_info.o

INF_OBJ=$(BUILDDIR_BIN)/gminfo.o \
        $(BUILDDIR_BIN)/game_maker.o \
        $(BUILDDIR_BIN)/png_info.o

UPD_OBJ=$(BUILDDIR_BIN)/gmupdate.o \
        $(BUILDDIR_BIN)/game_maker.o \
        $(BUILDDIR_BIN)/png_info.o

RES_OBJ=$(BUILDDIR_BIN)/make_resource.o \
        $(BUILDDIR_BIN)/png_info.o

EXT_DEP=

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
else
ifeq ($(TARGET),darwin32)
	CC=clang
	CFLAGS=$(POSIX_CFLAGS)
	ARCH_FLAGS=-m32
	EXT_DEP=macpkg
else
ifeq ($(TARGET),darwin64)
	CC=clang
	CFLAGS=$(POSIX_CFLAGS)
	ARCH_FLAGS=-m64
	EXT_DEP=macpkg
endif
endif
endif
endif
endif
endif

.PHONY: all clean cook_serve_hoomans quick_patch gmdump gmupdate make_resource patch setup pkg

# keep intermediary files (e.g. hoomans_png.c) to
# do less redundant work (when cross compiling):
.SECONDARY:

all: cook_serve_hoomans quick_patch gmdump gmupdate gminfo

cook_serve_hoomans: $(BUILDDIR_BIN)/cook_serve_hoomans$(BINEXT)

quick_patch: $(BUILDDIR_BIN)/quick_patch$(BINEXT)

gmdump: $(BUILDDIR_BIN)/gmdump$(BINEXT)

gminfo: $(BUILDDIR_BIN)/gminfo$(BINEXT)

gmupdate: $(BUILDDIR_BIN)/gmupdate$(BINEXT)

make_resource: $(BUILDDIR_BIN)/make_resource$(BINEXT)

setup:
	mkdir -p $(BUILDDIR_BIN) $(BUILDDIR_SRC)

patch: $(BUILDDIR_BIN)/cook_serve_hoomans$(BINEXT)
	$<

pkg: VERSION=$(shell git describe --tags)
pkg: $(BUILDDIR_BIN)/utils-for-advanced-users-$(VERSION)-$(TARGET).zip $(EXT_DEP) cook_serve_hoomans

macpkg: VERSION=$(shell git describe --tags)
macpkg: $(BUILDDIR_BIN)/cook_serve_hoomans_$(VERSION)_mac.zip

$(BUILDDIR_BIN)/cook_serve_hoomans_$(VERSION)_mac.zip: \
		$(BUILDDIR_BIN)/cook_serve_hoomans \
		$(BUILDDIR_BIN)/cook_serve_hoomans.command \
		$(BUILDDIR_BIN)/open_with_cook_serve_hoomans.command \
		$(BUILDDIR_BIN)/README.txt
	mkdir -p $(BUILDDIR_BIN)/bin
	cp $(BUILDDIR_BIN)/cook_serve_hoomans $(BUILDDIR_BIN)/bin
	cd $(BUILDDIR_BIN); zip -r9 cook_serve_hoomans_$(VERSION)_mac.zip \
		bin \
		cook_serve_hoomans.command \
		open_with_cook_serve_hoomans.command \
		README.txt
	rm -r $(BUILDDIR_BIN)/bin

$(BUILDDIR_BIN)/%.command: osx/%.command
	cp $< $@

$(BUILDDIR_BIN)/README.txt: osx/README.txt
	cp $< $@

$(BUILDDIR_BIN)/utils-for-advanced-users-$(VERSION)-$(TARGET).zip: quick_patch gmdump gminfo gmupdate
	mkdir -p $(BUILDDIR_BIN)/utils-for-advanced-users-$(VERSION)-$(TARGET)
	cp \
		README.md \
		$(BUILDDIR_BIN)/quick_patch$(BINEXT) \
		$(BUILDDIR_BIN)/gmdump$(BINEXT) \
		$(BUILDDIR_BIN)/gminfo$(BINEXT) \
		$(BUILDDIR_BIN)/gmupdate$(BINEXT) \
		$(BUILDDIR_BIN)/utils-for-advanced-users-$(VERSION)-$(TARGET)
	cd $(BUILDDIR_BIN); zip -r9 utils-for-advanced-users-$(VERSION)-$(TARGET).zip \
		utils-for-advanced-users-$(VERSION)-$(TARGET)
	rm -r $(BUILDDIR_BIN)/utils-for-advanced-users-$(VERSION)-$(TARGET)

$(BUILDDIR_SRC)/%_png.c: images/%.png $(BUILDDIR_BIN)/make_resource$(BINEXT)
	$(BUILDDIR_BIN)/make_resource$(BINEXT) csh_$(shell basename $< .png) $< $(basename $@).h $@

$(BUILDDIR_SRC)/%_png.h: $(BUILDDIR_SRC)/%_png.c;

$(BUILDDIR_BIN)/%.o: src/%.c
	$(CC) $(ARCH_FLAGS) $(CFLAGS) -c $< -o $@

$(BUILDDIR_BIN)/%.o: $(BUILDDIR_SRC)/%.c
	$(CC) $(ARCH_FLAGS) $(CFLAGS) -c $< -o $@

$(BUILDDIR_BIN)/cook_serve_hoomans.o: \
		src/cook_serve_hoomans.c \
		$(BUILDDIR_SRC)/hoomans_png.h \
		$(BUILDDIR_SRC)/catering_png.h \
		$(BUILDDIR_SRC)/icons_png.h
	$(CC) $(ARCH_FLAGS) $(CFLAGS) -c $< -o $@

$(BUILDDIR_BIN)/cook_serve_hoomans$(BINEXT): $(CSH_OBJ)
	$(CC) $(ARCH_FLAGS) $(CSH_OBJ) -o $@

$(BUILDDIR_BIN)/quick_patch$(BINEXT): $(QP_OBJ)
	$(CC) $(ARCH_FLAGS) $(QP_OBJ) -o $@

$(BUILDDIR_BIN)/gmdump$(BINEXT): $(DMP_OBJ)
	$(CC) $(ARCH_FLAGS) $(DMP_OBJ) -o $@

$(BUILDDIR_BIN)/gminfo$(BINEXT): $(INF_OBJ)
	$(CC) $(ARCH_FLAGS) $(INF_OBJ) -o $@

$(BUILDDIR_BIN)/gmupdate$(BINEXT): $(UPD_OBJ)
	$(CC) $(ARCH_FLAGS) $(UPD_OBJ) -o $@

$(BUILDDIR_BIN)/make_resource$(BINEXT): $(RES_OBJ)
	$(CC) $(ARCH_FLAGS) $(RES_OBJ) -o $@

clean: VERSION=$(shell git describe --tags)
clean:
	rm -f \
		$(BUILDDIR_SRC)/hoomans_png.h \
		$(BUILDDIR_SRC)/hoomans_png.c \
		$(BUILDDIR_SRC)/catering_png.h \
		$(BUILDDIR_SRC)/catering_png.c \
		$(BUILDDIR_SRC)/icons_png.h \
		$(BUILDDIR_SRC)/icons_png.c \
		$(BUILDDIR_BIN)/patch_game.o \
		$(BUILDDIR_BIN)/cook_serve_hoomans.o \
		$(BUILDDIR_BIN)/quick_patch.o \
		$(BUILDDIR_BIN)/gmdump.o \
		$(BUILDDIR_BIN)/gminfo.o \
		$(BUILDDIR_BIN)/gmupdate.o \
		$(BUILDDIR_BIN)/hoomans_png.o \
		$(BUILDDIR_BIN)/catering_png.o \
		$(BUILDDIR_BIN)/icons_png.o \
		$(BUILDDIR_BIN)/game_maker.o \
		$(BUILDDIR_BIN)/png_info.o \
		$(BUILDDIR_BIN)/make_resource.o \
		$(BUILDDIR_BIN)/cook_serve_hoomans$(BINEXT) \
		$(BUILDDIR_BIN)/quick_patch$(BINEXT) \
		$(BUILDDIR_BIN)/gmdump$(BINEXT) \
		$(BUILDDIR_BIN)/gminfo$(BINEXT) \
		$(BUILDDIR_BIN)/gmupdate$(BINEXT) \
		$(BUILDDIR_BIN)/make_resource$(BINEXT) \
		$(BUILDDIR_BIN)/README.txt \
		$(BUILDDIR_BIN)/cook_serve_hoomans.command \
		$(BUILDDIR_BIN)/open_with_cook_serve_hoomans.command \
		$(BUILDDIR_BIN)/cook_serve_hoomans_$(VERSION)_mac.zip \
		$(BUILDDIR_BIN)/utils-for-advanced-users-$(VERSION)-$(TARGET).zip
