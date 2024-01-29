#
# Copyright (C) 2000-2004 Wolfgang Denk, DENX Software Engineering, wd@denx.de
#
# SPDX-License-Identifier: GPL-2.0
#

include $(TOPDIR)/config.mk

OBJS-y	= rt2880_eth.o gpio.o

ifeq ($(CONFIG_NETCONSOLE),y)
    OBJS-y	+= netconsole.o
endif

ifeq ($(RALINK_MDIO_ACCESS_FUN),y)
    OBJS-y	+= mii_mgr.o
endif

ifeq ($(CFG_ENV_IS_IN_SPI),y)
    OBJS-y	+= spi_rt6855A_flash.o
endif

ifeq ($(MTK_XHCI), ON)
    OBJS-y	+= usb/host/xhci.o usb/host/xhci-mem.o usb/host/xhci-ring.o
    OBJS-y	+= usb/host/xhci-mtk.o usb/host/xhci-mtk-power.o usb/host/mtk-phy.o usb/host/mtk-phy-ahb.o usb/host/mtk-phy-7621.o
endif

ifeq ($(RALINK_EPHY_TESTER), ON)
    OBJS-y   += phy_val.o
endif

include $(TOPDIR)/end.mk
