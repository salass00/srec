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

SRCS := src/main.c src/locale.c src/cli.o src/gui.o src/srec.c src/interfaces.c src/timer.c src/zmbv.c src/zmbv_altivec.c
OBJS := $(SRCS:.c=.o)

CATALOGS := catalogs/italian/SRec.catalog

.PHONY: all revision clean catalogs

all: $(TARGET)

$(TARGET): $(OBJS) libmkv/libmkv.a
	$(CC) $(LDFLAGS) -o $@.debug $^ $(LIBS)
	$(STRIP) -R.comment -o $@ $@.debug

src/main.o: src/locale.h include/locale_strings.h src/cli.h src/gui.h SRec_rev.h

src/locale.o: src/locale.h include/locale_strings.h

src/cli.o: src/cli.h src/locale.h include/locale_strings.h src/srec.h

src/gui.o: src/gui.h src/locale.h include/locale_strings.h src/interfaces.h src/srec.h SRec_rev.h

src/srec.o: src/srec.h src/interfaces.h src/timer.h include/libmkv.h src/zmbv.h SRec_rev.h

src/interfaces.o: src/interfaces.h

src/timer.o: src/timer.h

src/zmbv.o: src/srec.h src/zmbv.h src/interfaces.h

src/zmbv_altivec.o: src/zmbv.h
src/zmbv_altivec.o: CFLAGS += -maltivec

include/locale_strings.h: catalogs/SRec.cd
ifeq ($(SYSTEM),AmigaOS)
	CatComp CFILE $@ NOCODE $<
else
	catcomp --cfile $@ $<
endif

libmkv/libmkv.a:
	make -C libmkv

avilib-0.6.10/libavi.a:
	make -C avilib-0.6.10

catalogs: $(CATALOGS)

%.catalog: %.ct catalogs/SRec.cd
	catcomp --catalog $@ catalogs/SRec.cd $<

revision:
	bumprev $(VERSION) $(TARGET)

clean:
	make -C libmkv clean
	make -C avilib-0.6.10 clean
	$(RM) src/*.o $(TARGET).debug $(TARGET)

