#
# Copyright (C) 2000 Wolfgang Denk, DENX Software Engineering, wd@denx.de
#
# SPDX-License-Identifier: GPL-2.0
#

include $(TOPDIR)/config.mk

OBJS-y	= net.o eth.o

ifeq ($(RARP_SUPPORT),y)
OBJS-y	+= rarp.o
endif

ifeq ($(SNTP_SUPPORT),y)
OBJS-y	+= sntp.o
endif

ifeq ($(BOOTP_SUPPORT),y)
OBJS-y	+= bootp.o
endif

ifeq ($(TFTP_SUPPORT),y)
OBJS-y	+= tftp.o
endif

ifeq ($(HTTPD_SUPPORT),y)
OBJS-y	+= httpd.o
endif

include $(TOPDIR)/end.mk
