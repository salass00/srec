CROSS  := ppc-amigaos
CC     := $(CROSS)-gcc
AR     := $(CROSS)-ar
RANLIB := $(CROSS)-ranlib
RM     := rm -f

TARGET := libmkv.a

CFLAGS := -O2 -g -Wall -Wwrite-strings -Werror -I. -I../include

SRCS := attachments.c chapters.c ebml.c matroska.c md5.c tags.c tracks.c
OBJS := $(SRCS:.c=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(AR) -crv $@ $^
	$(RANLIB) $@

clean:
	$(RM) *.o libmkv.a

