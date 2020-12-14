CROSS   := ppc-amigaos
CC      := $(CROSS)-gcc
AR      := $(CROSS)-ar
RANLIB  := $(CROSS)-ranlib
STRIP   := $(CROSS)-strip
OBJCOPY := $(CROSS)-objcopy
RM      := rm -f

TARGET  := SRec
VERSION := 2

SYSTEM := $(shell uname -s)

CFLAGS  := -O2 -g -Wall -Wwrite-strings -Werror -Iinclude
LDFLAGS := -static
LIBS    := 

SREC_CFLAGS := -I. -DENABLE_CLUT

SREC_SRCS := src/main.c src/locale.c src/cli.c src/gui.c src/srec.c src/pointer.c \
        src/scale.c src/interfaces.c src/timer.c src/zmbv.c src/zmbv_ppc.c \
        src/zmbv_altivec.c
SREC_OBJS := $(SREC_SRCS:.c=.o)
SREC_DEPS := $(SREC_SRCS:.c=.d)

include libmkv/libmkv.make
include avilib-0.6.10/avilib.make

DEPS := $(SREC_DEPS) $(LIBMKV_DEPS) $(AVILIB_DEPS)

CTFILES  := $(wildcard catalogs/*/SRec.ct)
CATALOGS := $(CTFILES:.ct=.catalog)

.PHONY: all revision clean catalogs

all: $(TARGET) catalogs

%.o: %.c
	$(CC) -MM -MP -MT $*.d -MT $@ -MF $*.d $(CFLAGS) $<
	$(CC) $(CFLAGS) -c -o $@ $<

$(TARGET): $(SREC_OBJS) $(LIBMKV) $(AVILIB)
	$(CC) $(LDFLAGS) -o $@.debug $^ $(LIBS)
	$(STRIP) -R.comment -o $@ $@.debug

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
	$(OBJCOPY) -I binary -O elf32-amigaos -B powerpc --redefine-sym _binary_src_rgb2yuv_vert_spv_start=rgb2yuv_vert_start --redefine-sym _binary_src_rgb2yuv_vert_spv_end=rgb2yuv_vert_end --redefine-sym _binary_src_rgb2yuv_vert_spv_size=rgb2yuv_vert_size  $< $@

src/rgb2yuv_frag.o: src/rgb2yuv_frag.spv
	$(OBJCOPY) -I binary -O elf32-amigaos -B powerpc --redefine-sym _binary_src_rgb2yuv_frag_spv_start=rgb2yuv_frag_start --redefine-sym _binary_src_rgb2yuv_frag_spv_end=rgb2yuv_frag_end --redefine-sym _binary_src_rgb2yuv_frag_spv_size=rgb2yuv_frag_size $< $@

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
	$(RM) $(LIBMKV) $(LIBMKVDIR)/*.o
	$(RM) $(AVILIB) $(AVILIBDIR)/*.o
	$(RM) src/*.[od] $(TARGET).debug $(TARGET)

