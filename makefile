CC      = ppc-amigaos-gcc
AR      = ppc-amigaos-ar
RANLIB  = ppc-amigaos-ranlib
STRIP   = ppc-amigaos-strip
OBJCOPY = ppc-amigaos-objcopy

TARGET  = SRec
VERSION = 2

SYSTEM = $(shell uname -s)

CFLAGS  = -O3 -g -Wall -Wwrite-strings -Werror -Iinclude
LDFLAGS = -static
LIBS    = 

STRIPFLAGS = -R.comment --strip-unneeded-rel-relocs

OBJCOPYFLAGS = -I binary -O elf32-amigaos -B powerpc

SREC_CFLAGS = -I. -DENABLE_CLUT

SREC_SRCS = src/main.c src/locale.c src/cli.c src/gui.c src/srec.c src/pointer.c \
            src/scale.c src/interfaces.c src/timer.c src/zmbv.c src/zmbv_ppc.c \
            src/zmbv_altivec.c
SREC_OBJS = $(SREC_SRCS:.c=.o)
SREC_DEPS = $(SREC_SRCS:.c=.d)

include libmkv/libmkv.make
include avilib-0.6.10/avilib.make

DEPS = $(SREC_DEPS) $(LIBMKV_DEPS) $(AVILIB_DEPS)

CTFILES  = $(wildcard catalogs/*/SRec.ct)
CATALOGS = $(CTFILES:.ct=.catalog)

.PHONY: all revision clean catalogs

all: $(TARGET) catalogs

%.o: %.c
	$(CC) -MM -MP -MT $*.d -MT $@ -MF $*.d $(CFLAGS) $<
	$(CC) $(CFLAGS) -c -o $@ $<

$(TARGET): $(SREC_OBJS) $(LIBMKV) $(AVILIB)
	$(CC) $(LDFLAGS) -o $@.debug $^ $(LIBS)
	$(STRIP) $(STRIPFLAGS) -o $@ $@.debug

$(SREC_OBJS): CFLAGS += $(SREC_CFLAGS)

src/zmbv_altivec.o: CFLAGS += -maltivec

src/main.o src/cli.o src/gui.o src/locale.o: include/locale_strings.h

include/locale_strings.h: catalogs/SRec.cd
ifeq ($(SYSTEM),AmigaOS)
	CatComp CFILE $@ NOCODE $<
else
	catcomp --cfile $@ $<
endif

src/rgb2yuv_vert.spv: src/rgb2yuv.vert
	glslangValidator -G -o $@ $<

src/rgb2yuv_frag.spv: src/rgb2yuv.frag
	glslangValidator -G -o $@ $<

src/rgb2yuv_vert.o: src/rgb2yuv_vert.spv
	$(OBJCOPY) $(OBJCOPYFLAGS) --redefine-syms=src/rgb2yuv_vert.sym $< $@

src/rgb2yuv_frag.o: src/rgb2yuv_frag.spv
	$(OBJCOPY) $(OBJCOPYFLAGS) --redefine-syms=src/rgb2yuv_frag.sym $< $@

$(LIBMKV): $(LIBMKV_OBJS)
	$(AR) -crv $@ $^
	$(RANLIB) $@

$(LIBMKV_OBJS): CFLAGS += $(LIBMKV_CFLAGS)

$(AVILIB): $(AVILIB_OBJS)
	$(AR) -crv $@ $^
	$(RANLIB) $@

$(AVILIB_OBJS): CFLAGS += $(AVILIB_CFLAGS)

-include $(DEPS)

%.catalog: %.ct catalogs/SRec.cd
	-flexcat CATALOG $@ catalogs/SRec.cd $<

catalogs: $(CATALOGS)

revision:
	bumprev $(VERSION) $(TARGET)

clean:
	rm -f $(LIBMKV) $(LIBMKVDIR)/*.o
	rm -f $(AVILIB) $(AVILIBDIR)/*.o
	rm -f src/*.[od] $(TARGET).debug $(TARGET)

