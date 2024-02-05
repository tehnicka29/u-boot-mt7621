#
# (C) Copyright 2000-2004
# Wolfgang Denk, DENX Software Engineering, wd@denx.de.
#
# See file CREDITS for list of people who contributed to this
# project.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation; either version 2 of
# the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston,
# MA 02111-1307 USA
#

HOSTARCH := $(shell uname -m | \
	sed -e s/i.86/i386/ \
	    -e s/sun4u/sparc64/ \
	    -e s/arm.*/arm/ \
	    -e s/sa110/arm/ \
	    -e s/powerpc/ppc/ \
	    -e s/macppc/ppc/)

HOSTOS := $(shell uname -s | tr A-Z a-z | \
	    sed -e 's/\(cygwin\).*/cygwin/')

export	HOSTARCH

# Deal with colliding definitions from tcsh etc.
#VENDOR=

#########################################################################

TOPDIR	:= $(shell if [ "$$PWD" != "" ]; then echo $$PWD; else pwd; fi)
export	TOPDIR

ifeq ($(BUILD_DIR),)
    BUILD_DIR := $(TOPDIR)/build
endif

ifeq ($(OUT_DIR),)
    OUT_DIR := $(BUILD_DIR)
endif

export BUILD_DIR OUT_DIR

CONFIG_CROSS_COMPILER_PATH ?= "$(HOME)/toolchain/mips-2021.02/bin"

ifeq (include/config.mk,$(wildcard include/config.mk))
# load ARCH, BOARD, and CPU configuration
include include/config.mk
export	ARCH CPU BOARD VENDOR SOC

# load other configuration
include $(TOPDIR)/config.mk

ifndef CROSS_COMPILE
ifeq ($(ARCH),mips)
    #CROSS_COMPILE = $(CONFIG_CROSS_COMPILER_PATH)/mipsel-openwrt-linux-
    CROSS_COMPILE = $(CONFIG_CROSS_COMPILER_PATH)/mipsel-linux-
endif
endif

ifeq ($(DEBUG_BUILD),y)
    STAGE1_BINARY=mt7621_stage_sram.bin
else
    STAGE1_BINARY=mt7621_stage_sram_noprint.bin
endif

DRAM_TYPE=DDR3

DRAM_SIZE=2
ifeq ($(ON_BOARD_64M_DRAM_COMPONENT),y)
    DRAM_SIZE=8
endif
ifeq ($(ON_BOARD_128M_DRAM_COMPONENT),y)
    DRAM_SIZE=16
endif
ifeq ($(ON_BOARD_256M_DRAM_COMPONENT),y)
    DRAM_SIZE=32
endif
ifeq ($(ON_BOARD_512M_DRAM_COMPONENT),y)
    DRAM_SIZE=64
endif
ifeq ($(ON_BOARD_1024M_DRAM_COMPONENT),y)
    DRAM_SIZE=128
endif
ifeq ($(ON_BOARD_1024M_KGD_DRAM_COMPONENT),y)
    DRAM_SIZE=128
endif
ifeq ($(ON_BOARD_2048M_DRAM_COMPONENT),y)
    DRAM_SIZE=256
endif
ifeq ($(ON_BOARD_4096M_DRAM_COMPONENT),y)
    DRAM_SIZE=448
endif

# DRAM_WIDTH: SDR(DDR2)
# 0:16(8)
# 1:32(16)
ifeq ($(ON_BOARD_DDR_WIDTH_16),y)
    DRAM_WIDTH=32
else
    DRAM_WIDTH=16
endif

ifeq ($(ON_BOARD_32BIT_DRAM_BUS),y)
    DRAM_TOTAL_WIDTH=32
else
    DRAM_TOTAL_WIDTH=16
endif

export CROSS_COMPILE
export STAGING_DIR = $(TOPDIR)/tmp

#########################################################################

LIBS = cpu/$(CPU) common
LIBS += lib_generic lib_generic/perf
LIBS += board/$(BOARDDIR)
ifdef SOC
    LIBS += cpu/$(CPU)/$(SOC)
endif
LIBS += lib_$(ARCH)
LIBS += net drivers
ifeq ($(HTTPD_SUPPORT),y)
    LIBS += httpd
endif

ifeq ($(RALINK_USB),ON)
    LIBS += fs/fat
    LIBS += disk
endif

LIBS := $(addsuffix /.obj.lst,$(LIBS))
LIBS := $(addprefix $(obj),$(LIBS))

LIBL := $(addprefix $(BUILD_DIR)/,$(LIBS))

.PHONY : $(LIBS)

#########################################################################
#########################################################################

ALL = uboot.bin uboot_a.bin System.map

all:		clean uboot_a.bin System.map

makedirs:
		@if [ ! -d $(BUILD_DIR) ]; then mkdir -p $(BUILD_DIR)/include; fi
		@if [ ! -d $(BUILD_DIR)/include/asm ]; then ln -s $(TOPDIR)/include/asm-$(ARCH) $(BUILD_DIR)/include/asm; fi

uboot_a.bin:	uboot.bin $(STAGE1_BINARY)
		cp uboot.bin uboot_a.bin
		chmod 666 uboot_a.bin
		dd if=$(STAGE1_BINARY) of=uboot_a.bin bs=1 count=$(shell stat -c %s $(STAGE1_BINARY)) \
		seek=$(shell stat -c %s uboot.bin)
ifeq ($(MT7621_CPU_800MHZ), y)
		echo "0 725a00c0"|xxd -r|dd bs=1 of=uboot_a.bin seek=$(shell echo "(($(shell stat -c %s uboot.bin)+32))" |bc)  conv=notrunc
endif
# ifeq ($(MT7621_CPU_880MHZ), y)
#		echo "0 b25a00c0"|xxd -r|dd bs=1 of=uboot_a.bin seek=$(shell echo "(($(shell stat -c %s uboot.bin)+32))" |bc)  conv=notrunc
# endif
ifeq ($(MT7621_CPU_900MHZ), y)
		echo "0 c25a00c0"|xxd -r|dd bs=1 of=uboot_a.bin seek=$(shell echo "(($(shell stat -c %s uboot.bin)+32))" |bc)  conv=notrunc
endif
ifeq ($(MT7621_CPU_875MHZ), y)
		echo "0 224a00c0"|xxd -r|dd bs=1 of=uboot_a.bin seek=$(shell echo "(($(shell stat -c %s uboot.bin)+32))" |bc)  conv=notrunc
#		echo "0 a25a00c0"|xxd -r|dd bs=1 of=uboot_a.bin seek=$(shell echo "(($(shell stat -c %s uboot.bin)+32))" |bc)  conv=notrunc
endif
		echo "0 $(MT7621_DDR_SPEED)"|xxd -r|dd bs=1 count=1 of=uboot_a.bin seek=$(shell echo "(($(shell stat -c %s uboot.bin)+39))" |bc) conv=notrunc
ifeq ($(DDR_ACT_SETTING), y)
		sh mt7621_ddr.sh uboot_a.bin uboot_a.bin mt7621_ddr_param.txt $(DDR_CHIP) IN_SPI || exit 1
		echo "0 10"|xxd -r|dd bs=1 count=1 of=uboot_a.bin seek=$(shell echo "(($(shell stat -c %s uboot.bin)+38))" |bc) conv=notrunc
endif
ifneq ($(CONFIG_BAUDRATE_57600), y)
		echo "0 00c20100"|xxd -r|dd bs=1 of=uboot_a.bin seek=$(shell echo "(($(shell stat -c %s uboot.bin)+304))" |bc) conv=notrunc
endif
		mv uboot_a.bin uboot.bin

uboot.bin:	u-boot
		$(Q)$(OBJCOPY) $(OBJCFLAGS) -O binary $< $@

u-boot:		autoconf.h depend makedirs $(SUBDIRS) $(OBJS) $(LIBS) $(LDSCRIPT)
		$(Q)$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) $(shell cat $(LIBL)) $(PLATFORM_LIBS) -Wl,-Map,u-boot.map -o u-boot

$(LIBS):
#		$(Q)echo $(MAKE) -C `dirname $@`
		$(Q)$(MAKE) -s -C `dirname $@`

$(SUBDIRS):
#		$(Q)echo $(MAKE) -C $@ all
		$(MAKE) -C $@ all

depend dep:
		@for dir in $(SUBDIRS) ; do $(Q)$(MAKE) -C $$dir .depend ; done

System.map:	u-boot
		@$(NM) $< | \
		grep -v '\(compiled\)\|\(\.o$$\)\|\( [aUw] \)\|\(\.\.ng$$\)\|\(LASH[RL]DI\)' | \
		sort > System.map

#########################################################################
else
all install u-boot u-boot.srec depend dep:
	@echo "System not configured - see README" >&2
	@ exit 1
endif

#########################################################################

unconfig:
	@rm -f board/*/config.tmp .config .config.old .tmp_config .menuconfig.log autoconf.h

#========================================================================
# MIPS
#========================================================================

#########################################################################
## MIPS32r2 1004kc
#########################################################################

archer_ax23_config: unconfig
	@cp $(TOPDIR)/../profiles/archer_ax23_config $(TOPDIR)/.config

#########################################################################

clean:
	@rm -rf $(BUILD_DIR)
	@find . -type f \
		\( -name 'core' -o -name '*.bak' -o -name '*~' \
		-o -name '*.o'  -o -name '*.a'  \) -print \
		| xargs rm -f
	@find ./ -name '.depend' -print | xargs rm -f
	@rm -f System.map u-boot.map uboot.bin u-boot
	@rm -f scripts/lxdialog/lxdialog
	@sync

clobber:	clean
	find . -type f \( -name .depend \
		-o -name 'uboot.bin' -o -name u-boot.img \) \
		-print0 \
		| xargs -0 rm -f
	rm -f $(OBJS) *.bak tags TAGS
	rm -fr *.*~
	rm -f u-boot u-boot.map u-boot.hex $(ALL)

mrproper \
distclean:	clobber unconfig

backup: clean
	F=`basename $(TOPDIR)` ; cd .. ; \
	tar --force-local -J -cvf `date "+$$F-%Y-%m-%d-%T.tar.xz"` $$F

menuconfig: config.in
	$(MAKE) -C scripts/lxdialog all
	$(CONFIG_SHELL) scripts/Menuconfig config.in

autoconf.h oldconfig:
	$(CONFIG_SHELL) scripts/oldconfig config.in

.PHONY: clean clobber mrproper distclean unconfig menuconfig oldconfig

#########################################################################
