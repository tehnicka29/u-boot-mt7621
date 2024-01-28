
ifeq ($(CURDIR),$(TOPDIR))
    dir :=
    obj :=
else
    dir := $(subst $(TOPDIR)/,,$(CURDIR))
    obj := $(BUILD_DIR)/$(dir)/
endif

#####################################################################
# Please executes 'make menuconfig' to config your own uboot
#####################################################################
-include $(TOPDIR)/.config

#################<< Test function option Configuration >>###################

#----------------------------------------
#If using an 32MB flash on RT3052_MP2, you might try to turn this on.
#----------------------------------------
#ON_BOARD_32M_FLASH_COMPONENT = y

######## RT2880 test function option configuration ##########################
RALINK_SPI_UPGRADE_CHECK = ON
RALINK_USB = OFF
MTK_XHCI = OFF
RALINK_SSO_TEST_FUN = OFF
RALINK_VITESSE_SWITCH_CONNECT_SPI_CS1 = OFF
RALINK_SPI_CS0_HIGH_ACTIVE = OFF
RALINK_SPI_CS1_HIGH_ACTIVE = OFF

ifeq ($(USB_RECOVERY_SUPPORT),y)
MTK_XHCI = ON
RALINK_USB = ON
endif

#Only for built-in 10/100/1000 Embedded Switch
RALINK_EPHY_TESTER = OFF

#Only for built-in 10/100 Embedded Switch
RALINK_SWITCH_DEBUG_FUN = OFF

##############################
# Decompression Algorithm
##############################
CONFIG_GZIP = OFF
CONFIG_LZMA = ON

##########################################################################

# clean the slate ...
PLATFORM_RELFLAGS =
PLATFORM_CPPFLAGS =
PLATFORM_LDFLAGS =

ifdef	ARCH
sinclude $(TOPDIR)/$(ARCH)_config.mk	# include architecture dependend rules
endif
ifdef	CPU
sinclude $(TOPDIR)/cpu/$(CPU)/config.mk	# include  CPU	specific rules
endif
ifdef	SOC
sinclude $(TOPDIR)/cpu/$(CPU)/$(SOC)/config.mk	# include  SoC	specific rules
endif
ifdef	VENDOR
BOARDDIR = $(VENDOR)/$(BOARD)
else
BOARDDIR = $(BOARD)
endif
ifdef	BOARD
sinclude $(TOPDIR)/board/$(BOARDDIR)/config.mk	# include board specific rules
endif

#########################################################################

CONFIG_SHELL	:= $(shell if [ -x "$$BASH" ]; then echo $$BASH; \
		    else if [ -x /bin/bash ]; then echo /bin/bash; \
		    else echo sh; fi ; fi)

ifeq ($(HOSTOS)-$(HOSTARCH),darwin-ppc)
	HOSTCC		= cc
else
	HOSTCC		= gcc
endif
HOSTCFLAGS	= -Wall -Wstrict-prototypes -g -fomit-frame-pointer
HOSTSTRIP	= strip

#########################################################################

#
# Include the make variables (CC, etc...)
#
AS	= $(CROSS_COMPILE)as
LD	= $(CROSS_COMPILE)ld
CC	= $(CROSS_COMPILE)gcc
CPP	= $(CC) -E
AR	= $(CROSS_COMPILE)ar
NM	= $(CROSS_COMPILE)nm
STRIP	= $(CROSS_COMPILE)strip
OBJCOPY = $(CROSS_COMPILE)objcopy
OBJDUMP = $(CROSS_COMPILE)objdump
RANLIB	= $(CROSS_COMPILE)RANLIB

RELFLAGS= $(PLATFORM_RELFLAGS)
ifeq ($(DEBUG_BUILD),y)
DBGFLAGS= -gdwarf-2 -DDEBUG
else
DBGFLAGS=
endif
ifndef LDSCRIPT
    LDSCRIPT := $(TOPDIR)/board/$(BOARDDIR)/u-boot.lds
endif
OBJCFLAGS += --gap-fill=0xff

gccincdir := $(shell $(CC) -print-file-name=include)

CPPFLAGS := $(DBGFLAGS) $(RELFLAGS)		\
	-D__KERNEL__ -I$(TOPDIR)/include	\
	-fno-builtin -ffreestanding -nostdinc 	\
	-I$(gccincdir) $(PLATFORM_CPPFLAGS)     \
	-I$(BUILD_DIR)/include

#ifneq ($(dir),)
#  CPPFLAGS += -I$(dir)/include -I$(dir)
#endif

ifeq ($(UN_NECESSITY_U_BOOT_CMD_OPEN),ON)
CPPFLAGS += -DRT2880_U_BOOT_CMD_OPEN
endif

ifeq ($(RALINK_SWITCH_DEBUG_FUN),ON)
CPPFLAGS += -DRALINK_SWITCH_DEBUG_FUN
endif

ifeq ($(RALINK_SPI_UPGRADE_CHECK),ON)
CPPFLAGS += -DRALINK_SPI_UPGRADE_CHECK
endif

ifeq ($(RALINK_RW_RF_REG_FUN),ON)
CPPFLAGS += -DRALINK_RW_RF_REG_FUN
endif

ifeq ($(RALINK_VITESSE_SWITCH_CONNECT_SPI_CS1),ON)
CPPFLAGS += -DRALINK_VITESSE_SWITCH_CONNECT_SPI_CS1
endif

ifeq ($(RALINK_SPI_CS0_HIGH_ACTIVE),ON)
CPPFLAGS += -DRALINK_SPI_CS0_HIGH_ACTIVE
endif

ifeq ($(RALINK_SPI_CS1_HIGH_ACTIVE),ON)
CPPFLAGS += -DRALINK_SPI_CS1_HIGH_ACTIVE
endif

ifeq ($(RALINK_EPHY_TESTER),ON)
CPPFLAGS += -DRALINK_EPHY_TESTER
endif

ifeq ($(RALINK_CMDLINE),ON)
CPPFLAGS += -DRALINK_CMDLINE
endif

ifeq ($(DEBUG_BUILD),y)
CPPFLAGS += -DCONFIG_UNCOMPRESS_TIME
endif

ifeq ($(MTK_XHCI),ON)
CPPFLAGS += -DMTK_USB -DCONFIG_RALINK_MT7621 -DCONFIG_USB_XHCI
MTK_USB = ON
# save size for XHCI code
# RALINK_UPGRADE_BY_SERIAL = OFF
RALINK_RW_RF_REG_FUN = OFF
endif

ifeq ($(RALINK_SSO_TEST_FUN),ON)
CPPFLAGS += -DRALINK_SSO_TEST_FUN
endif

ifeq ($(GPIOx_RESET_MODE),y)
CPPFLAGS += -DGPIOx_RESET_MODE
endif

ifeq ($(ON_BOARD_16M_DRAM_COMPONENT),y)
CPPFLAGS += -DON_BOARD_16M_DRAM_COMPONENT
else
ifeq ($(ON_BOARD_64M_DRAM_COMPONENT),y)
CPPFLAGS += -DON_BOARD_64M_DRAM_COMPONENT
else
ifeq ($(ON_BOARD_128M_DRAM_COMPONENT),y)
CPPFLAGS += -DON_BOARD_128M_DRAM_COMPONENT
else
ifeq ($(ON_BOARD_256M_DRAM_COMPONENT),y)
CPPFLAGS += -DON_BOARD_256M_DRAM_COMPONENT
else
ifeq ($(ON_BOARD_512M_DRAM_COMPONENT),y)
CPPFLAGS += -DON_BOARD_512M_DRAM_COMPONENT
else
ifeq ($(ON_BOARD_1024M_DRAM_COMPONENT),y)
CPPFLAGS += -DON_BOARD_1024M_DRAM_COMPONENT
else
ifeq ($(ON_BOARD_2048M_DRAM_COMPONENT),y)
CPPFLAGS += -DON_BOARD_2048M_DRAM_COMPONENT
else
ifeq ($(ON_BOARD_4096M_DRAM_COMPONENT),y)
CPPFLAGS += -DON_BOARD_4096M_DRAM_COMPONENT
endif
endif
endif
endif
endif
endif
endif
endif

ifdef BUILD_TAG
	CPPFLAGS += -DBUILD_TAG='"$(BUILD_TAG)"'
endif

CFLAGS := -pipe -Os -EL -Wall -Wstrict-prototypes
CFLAGS += -Wno-unused -Wno-unused-variable -Wno-unused-but-set-variable -Wno-pointer-sign
CFLAGS += -fPIE -ffunction-sections
CFLAGS += -fno-jump-tables

ifeq ($(CONFIG_LTO),y)
# use LTO, FIXME: discards LZMA speed optimizations with MIPS16 and hangs at boot?
    CFLAGS += -flto -ffat-lto-objects
endif

AFLAGS_DEBUG := -Wa,-gstabs
AFLAGS := $(AFLAGS_DEBUG) -EL
AFLAGS += -G0 -mips32r2 -mmt -fno-lto -mno-mips16

CFLAGS += -G0 -mips32r2 -fomit-frame-pointer
CFLAGS += -mtune=1004kc

ifeq ($(CONFIG_MIPS16_ASE),y)
    CFLAGS += -mips16 -minterlink-mips16
endif

LDFLAGS += --static -nostdlib -T $(LDSCRIPT) -Wl,-Ttext,$(TEXT_BASE) $(PLATFORM_LDFLAGS)
LDFLAGS += -Wl,--gc-sections -fwhole-program

#########################################################################
export	CONFIG_SHELL HPATH HOSTCC HOSTCFLAGS CROSS_COMPILE \
	AS LD CC CPP AR NM STRIP OBJCOPY OBJDUMP \
	MAKE
export	TEXT_BASE PLATFORM_CPPFLAGS PLATFORM_RELFLAGS CPPFLAGS CFLAGS AFLAGS
#########################################################################

ifeq ($(V),)
    Q=@
else
    Q=
endif
export Q

$(obj)%.o: %.c
	@mkdir -p $(dir $@)
	@echo " [CC] $(CURDIR)/$<"
	$(Q)$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -c -o $@ $<

$(obj)%.o: %.S
	@mkdir -p $(dir $@)
	@echo " [AS] $(CURDIR)/$<"
	$(Q)$(CC) -D__ASSEMBLY__ $(CPPFLAGS) $(AFLAGS) -MMD -c -o $@ $<

#########################################################################
