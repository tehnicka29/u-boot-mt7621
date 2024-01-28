#
# library functions that might be sensible to execution speed
#

include $(TOPDIR)/config.mk

OBJS-y	=

ifeq ($(CONFIG_GZIP),ON)
OBJS-y   += zlib.o
endif

ifeq ($(CONFIG_LZMA),ON)
OBJS-y   += LzmaDecode.o
endif

CFLAGS += -O3 -mtune=1004kc -mno-mips16

include $(TOPDIR)/end.mk
