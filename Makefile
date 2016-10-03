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

SRCS := src/main.c src/locale.c src/cli.c src/gui.c src/srec.c src/interfaces.c src/timer.c src/zmbv.c src/zmbv_altivec.c
OBJS := $(SRCS:.c=.o)
DEPS := $(SRCS:.c=.d)

AVILIB := avilib-0.6.10/libavi.a
LIBMKV := libmkv/libmkv.a

CTFILES  := $(wildcard catalogs/*/SRec.ct)
CATALOGS := $(CTFILES:.ct=.catalog)

.PHONY: all revision clean catalogs

all: $(TARGET)

$(TARGET): $(OBJS) $(AVILIB) $(LIBMKV)
	$(CC) $(LDFLAGS) -o $@.debug $^ $(LIBS)
	$(STRIP) -R.comment -o $@ $@.debug

%.o: %.c
	$(CC) -MM -MP -MT $*.d -MT $@ -MF $*.d $(CFLAGS) $<
	$(CC) $(CFLAGS) -c -o $@ $<

src/zmbv_altivec.o: CFLAGS += -maltivec

-include $(DEPS)

include/locale_strings.h: catalogs/SRec.cd
ifeq ($(SYSTEM),AmigaOS)
	CatComp CFILE $@ NOCODE $<
else
	catcomp --cfile $@ $<
endif

$(LIBMKV):
	make -C $(dir $@)

$(AVILIB):
	make -C $(dir $@)

catalogs: $(CATALOGS)

%.catalog: %.ct catalogs/SRec.cd
	catcomp --catalog $@ catalogs/SRec.cd $<

revision:
	bumprev $(VERSION) $(TARGET)

clean:
	make -C libmkv clean
	make -C avilib-0.6.10 clean
	$(RM) src/*.o $(TARGET).debug $(TARGET)

