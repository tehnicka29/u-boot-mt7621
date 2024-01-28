#
# Makefile for http stuff
#

include $(TOPDIR)/config.mk

# Suppress warnings when building with size optimization
CFLAGS += -fno-strict-aliasing

CFLAGS += -I$(obj)

OBJS-y += uip.o uip_arch.o uip_arp.o httpd.o fs.o

include $(TOPDIR)/end.mk

fs.c:	$(obj)fsdata.c

$(obj)fsdata.c:
	bash vendors/makefsdatac $(obj)fsdata.c
