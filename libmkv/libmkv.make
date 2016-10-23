LIBMKVDIR := libmkv

LIBMKV := $(LIBMKVDIR)/libmkv.a

LIBMKV_CFLAGS := -I$(LIBMKVDIR)

LIBMKV_SRCS := $(LIBMKVDIR)/attachments.c $(LIBMKVDIR)/chapters.c \
               $(LIBMKVDIR)/ebml.c $(LIBMKVDIR)/matroska.c \
               $(LIBMKVDIR)/md5.c $(LIBMKVDIR)/tags.c $(LIBMKVDIR)/tracks.c
LIBMKV_OBJS := $(LIBMKV_SRCS:.c=.o)
LIBMKV_DEPS := $(LIBMKV_SRCS:.c=.d)

