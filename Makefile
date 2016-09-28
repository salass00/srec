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

.PHONY: all revision clean

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

include/locale_strings.h: SRec.cd
ifeq ($(SYSTEM),AmigaOS)
	CatComp CFILE $@ NOCODE $<
else
	catcomp --cfile $@ $<
endif

libmkv/libmkv.a:
	make -C libmkv

revision:
	bumprev $(VERSION) $(TARGET)

clean:
	make -C libmkv clean
	$(RM) src/*.o $(TARGET).debug $(TARGET)

