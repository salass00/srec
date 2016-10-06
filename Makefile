CROSS  := ppc-amigaos
CC     := $(CROSS)-gcc
STRIP  := $(CROSS)-strip
RM     := rm -f

TARGET  := SRec
VERSION := 2

SYSTEM := $(shell uname -s)

CFLAGS  := -O2 -g -Wall -Wwrite-strings -Werror -I. -Iinclude
LDFLAGS := -static
LIBS    := 

SRCS := src/main.c src/locale.c src/cli.c src/gui.c src/srec.c src/pointer.c \
        src/interfaces.c src/timer.c src/zmbv.c src/zmbv_altivec.c
OBJS := $(SRCS:.c=.o)
DEPS := $(SRCS:.c=.d)

AVILIB := avilib-0.6.10/libavi.a
LIBMKV := libmkv/libmkv.a

CTFILES  := $(wildcard catalogs/*/SRec.ct)
CATALOGS := $(CTFILES:.ct=.catalog)

.PHONY: all revision clean catalogs

all: $(TARGET) catalogs

%.o: %.c
	$(CC) -MM -MP -MT $*.d -MT $@ -MF $*.d $(CFLAGS) $<
	$(CC) $(CFLAGS) -c -o $@ $<

$(TARGET): $(OBJS) $(AVILIB) $(LIBMKV)
	$(CC) $(LDFLAGS) -o $@.debug $^ $(LIBS)
	$(STRIP) -R.comment -o $@ $@.debug

src/zmbv_altivec.o: CFLAGS += -maltivec

src/main.o src/cli.o src/gui.o src/locale.o: include/locale_strings.h

include/locale_strings.h: catalogs/SRec.cd
ifeq ($(SYSTEM),AmigaOS)
	CatComp CFILE $@ NOCODE $<
else
	catcomp --cfile $@ $<
endif

-include $(DEPS)

$(LIBMKV):
	make -C $(dir $@)

$(AVILIB):
	make -C $(dir $@)

%.catalog: %.ct catalogs/SRec.cd
	catcomp --catalog $@ catalogs/SRec.cd $<

catalogs: $(CATALOGS)

revision:
	bumprev $(VERSION) $(TARGET)

clean:
	make -C libmkv clean
	make -C avilib-0.6.10 clean
	$(RM) src/*.[od] $(TARGET).debug $(TARGET)

