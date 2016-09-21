CROSS  := ppc-amigaos
CC     := $(CROSS)-gcc
STRIP  := $(CROSS)-strip
RM     := rm -f

TARGET  := SRec
VERSION := 2

CFLAGS  := -O2 -g -Wall -Wwrite-strings -Werror -I. -Iinclude
LDFLAGS := -static
LIBS    := 

SRCS := srec/main.c srec/locale.c srec/cli.o srec/gui.o srec/srec.c srec/interfaces.c srec/timer.c srec/zmbv.c
OBJS := $(SRCS:.c=.o)

.PHONY: all revision clean

all: $(TARGET)

$(TARGET): $(OBJS) libmkv/libmkv.a
	$(CC) $(LDFLAGS) -o $@.debug $^ $(LIBS)
	$(STRIP) -R.comment -o $@ $@.debug

srec/main.o: srec/locale.h srec/cli.h srec/gui.h SRec_rev.h

srec/locale.o: srec/locale.h

srec/cli.o: srec/cli.h srec/locale.h srec/srec.h

srec/gui.o: srec/gui.h srec/locale.h srec/interfaces.h srec/srec.h SRec_rev.h

srec/srec.o: srec/srec.h srec/interfaces.h srec/timer.h include/libmkv.h srec/zmbv.h SRec_rev.h

srec/interfaces.o: srec/interfaces.h

srec/timer.o: srec/timer.h

srec/zmbv.o: srec/zmbv.h srec/interfaces.h

srec/locale.h: include/locale_strings.h

include/locale_strings.h: SRec.cd
	catcomp --cfile $@ $<

libmkv/libmkv.a:
	make -C libmkv

revision:
	bumprev $(VERSION) $(TARGET)

clean:
	make -C libmkv clean
	$(RM) srec/*.o $(TARGET).debug $(TARGET)

