CROSS  := ppc-amigaos
CC     := $(CROSS)-gcc
AR     := $(CROSS)-ar
RANLIB := $(CROSS)-ranlib
RM     := rm -f

TARGET := libavi.a

CFLAGS := -O2 -g -Wall -Wwrite-strings -Werror -I. -I../include -DWORDS_BIGENDIAN

SRCS := avilib.c xio.c
OBJS := $(SRCS:.c=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(AR) -crv $@ $^
	$(RANLIB) $@

avilib.o: ../include/avilib.h xio.h

xio.o: xio.h

clean:
	$(RM) *.o libavi.a

