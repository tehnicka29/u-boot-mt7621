#
# Copyright (C) 2000-2004 Wolfgang Denk, DENX Software Engineering, wd@denx.de
#
# SPDX-License-Identifier: GPL-2.0
#

include $(TOPDIR)/config.mk

ifeq ($(RALINK_CMDLINE),y)
OBJS-y	= main.o cmd_bdinfo.o cmd_boot.o cmd_bootm.o cmd_console.o \
	cmd_misc.o cmd_net.o cmd_mem.o cmd_custom.o \
	cmd_nvedit.o command.o console.o devices.o \
	env_common.o exports.o lists.o
else
OBJS-y = console.o devices.o \
	env_common.o exports.o lists.o cmd_nvedit.o cmd_bootm.o cmd_net.o
endif

ifeq ($(TFTP_SUPPORT),y)
	OBJS-y += cmd_tftpServer.o
endif

ifeq ($(RALINK_UPGRADE_BY_SERIAL),y)
	OBJS-y += cmd_load.o
endif

ifeq ($(RALINK_USB),ON)
	OBJS-y += usb.o usb_hub.o usb_storage.o cmd_usb.o cmd_fat.o
else
ifeq ($(MTK_USB),ON)
	OBJS-y += usb.o usb_hub.o usb_storage.o cmd_usb.o cmd_fat.o
endif
endif

OBJS-y += cmd_rootfs.o

ifeq ($(CONFIG_QMALLOC),y)
	OBJS-y += qmalloc.o
else
	OBJS-y += dlmalloc.o
endif

ifeq ($(CFG_ENV_IS_IN_SPI),y)
	OBJS-y	+= env_spi.o
endif

CPPFLAGS += -I..

include $(TOPDIR)/end.mk
