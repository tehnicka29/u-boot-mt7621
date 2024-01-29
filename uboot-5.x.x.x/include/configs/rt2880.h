/*
 * (C) Copyright 2003
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

/*
 * This file contains the configuration parameters for the RT2880 board.
 */

#ifndef __CONFIG_H
#define __CONFIG_H

#include "../../autoconf.h"

//#define DEBUG				1
//#define ET_DEBUG
#define CONFIG_RT2880_ETH		1	/* Enable built-in 10/100 Ethernet */

//#define CONFIG_CMD_HISTORY

#if defined (MT7621_CPU_FREQUENCY)
#define CPU_CLOCK_RATE	(MT7621_CPU_FREQUENCY*1000000)
#else
#define CPU_CLOCK_RATE  (800000000)
#endif

#define SERIAL_CLOCK_DIVISOR 16

#if defined (CONFIG_BAUDRATE_57600)
#define CONFIG_BAUDRATE		57600
#else
#define CONFIG_BAUDRATE		115200	/* 115200 by default */
#endif

//#define CONFIG_NETCONSOLE      1
#define CONFIG_NETCONSOLE_PORT 6666

/* valid baudrates */
#define CFG_BAUDRATE_TABLE	{ 9600, 19200, 38400, 57600, 115200 }

//#define	CONFIG_TIMESTAMP		/* Print image info with timestamp */

#include <cmd_confdefs.h>

/* Enable most commands */
#define RT2880_U_BOOT_CMD_OPEN

/*
 * Miscellaneous configurable options
 */
#define	CFG_LONGHELP				/* undef to save memory      */

#define	CFG_PROMPT		"MT7621 # "

#define	CFG_CBSIZE		256		/* Console I/O Buffer Size   */
#define	CFG_PBSIZE (CFG_CBSIZE+sizeof(CFG_PROMPT)+16)  /* Print Buffer Size */
#define	CFG_MAXARGS		16		/* max number of command args*/

#define CFG_MALLOC_LEN		256*1024

//#define CFG_BOOTPARAMS_LEN	128*1024
#define CFG_BOOTPARAMS_LEN	(16*1024) /* should be enough for our usage */

#define CFG_HZ			(CPU_CLOCK_RATE/2UL)

#define CFG_SDRAM_BASE		0x80000000

/*
 * for TEST
 */
#define CFG_CONSOLE_INFO_QUIET
#define	CFG_LOAD_ADDR		(CFG_SDRAM_BASE + (gd->ram_size)/2)	/* default load address	*/

#define CFG_HTTP_DL_ADDR        0x80300000
#define CFG_SPINAND_LOAD_ADDR   0x82000000
#define CFG_MEMTEST_START       0x80100000
#define CFG_MEMTEST_END         0x80400000

/*-----------------------------------------------------------------------
 * FLASH and environment organization
 */
#define PHYS_FLASH_START	0xBC000000 /* Flash Bank #2 */
#define PHYS_FLASH_1		0xBC000000 /* Flash Bank #1 */

/* The following #defines are needed to get flash environment right */
#define	CFG_MONITOR_BASE	TEXT_BASE

#define	CFG_MONITOR_LEN		(192 << 10)

#define CFG_INIT_SP_OFFSET	0x400000

#define CFG_FLASH_BASE		PHYS_FLASH_1

/* timeout values are in ticks */
#define CFG_FLASH_ERASE_TOUT         (15UL * CFG_HZ) /* Timeout for Flash Erase */
#define CFG_FLASH_WRITE_TOUT         (5 * CFG_HZ) /* Timeout for Flash Write */
#define CFG_ETH_AN_TOUT              (5 * CFG_HZ) /* Timeout for Flash Write */
#define CFG_ETH_LINK_UP_TOUT         (5 * CFG_HZ) /* Timeout for Flash Write */
#define CFG_FLASH_STATE_DISPLAY_TOUT (2 * CFG_HZ) /* Timeout for Flash Write */


#if defined (ON_BOARD_2M_FLASH_COMPONENT)
#define CFG_FLASH_SIZE		(0x200000)
#elif defined (ON_BOARD_4M_FLASH_COMPONENT)
#define CFG_FLASH_SIZE		(0x400000)
#elif defined (ON_BOARD_8M_FLASH_COMPONENT)
#define CFG_FLASH_SIZE		(0x800000)
#elif defined (ON_BOARD_16M_FLASH_COMPONENT)
#define CFG_FLASH_SIZE		(0x1000000)
#elif defined (ON_BOARD_32M_FLASH_COMPONENT)
#define CFG_FLASH_SIZE		(0x2000000)
#endif

#define CFG_BOOTLOADER_SIZE	0x30000 // default size (192k)
#define CFG_BOOTENV_SIZE	0x10000 // 64k, u-boot environment configuration

#define CFG_CONFIG_SIZE		0x10000 // "config" partition
#define CFG_TPLINK_SIZE		0x40000 // "tplink" partition

#define CFG_MISC_SIZE		(CFG_CONFIG_SIZE + CFG_TPLINK_SIZE) // 320k other partitions (config, etc), before radio
#define CFG_RADIO_SIZE		0x10000 // 64k, calibration data, at the end of the flash

#define CFG_BOOTLOADER_OFFSET 0x000000
#define CFG_BOOTENV_OFFSET    (CFG_BOOTLOADER_OFFSET + CFG_BOOTLOADER_SIZE)
#define CFG_KERNEL_OFFSET     (CFG_BOOTENV_OFFSET + CFG_BOOTENV_SIZE)

/* TP-Link specific */
#define CFG_CONFIG_OFFSET    0xFA0000
#define CFG_TPLINK_OFFSET    0xFB0000
#define CFG_RADIO_OFFSET     0xFF0000

/* Max potential rootfs offset plus one flash sector */
#define CFG_MAX_ROOTFS_OFFSET CFG_CONFIG_OFFSET

#define CFG_ENV_ADDR		(CFG_FLASH_BASE + CFG_BOOTLOADER_SIZE)
#define CFG_KERN_ADDR		(CFG_FLASH_BASE + (CFG_BOOTLOADER_SIZE + CFG_BOOTENV_SIZE))

/* U-Boot environment */
#define CFG_ENV_SECT_SIZE	CFG_BOOTENV_SIZE
#define CFG_ENV_SIZE		0x1000

#define LINUX_FILE_SIZE_MIN	0x80000

#define UBOOT_FILE_SIZE_MAX	(CFG_BOOTLOADER_SIZE + CFG_BOOTENV_SIZE)
#define UBOOT_FILE_SIZE_MIN	0x8000	/* for prevent flash damaged Uboot */

#define TFTP_UPLOAD_RAM_ADDRESS 0x80A00000

// httpd constants
#define WEBFAILSAFE_UPLOAD_LIMITED_AREA_IN_BYTES    (CFG_BOOTLOADER_SIZE + CFG_BOOTENV_SIZE + CFG_MISC_SIZE + CFG_RADIO_SIZE)
//#define WEBFAILSAFE_UPLOAD_RAM_ADDRESS              load_addr
#define WEBFAILSAFE_UPLOAD_RAM_ADDRESS              CFG_LOAD_ADDR /* more safe option than load_addr */
// httpd constants

#define CONFIG_NR_DRAM_BANKS	1
#define CFG_RX_ETH_BUFFER		60

/*-----------------------------------------------------------------------
 * Cache Configuration
 */
#define CFG_DCACHE_SIZE		(32*1024)
#define CFG_ICACHE_SIZE		(32*1024)
#define CFG_CACHELINE_SIZE	32

#define CONFIG_SYS_CACHELINE_SIZE 32
/*
 * System Controller	(0x00300000)
 *   Offset
 *   0x10  -- SYSCFG		System Configuration Register
 *   0x30  -- CLKCFG1		Clock Configuration Register
 *   0x34  -- RSTCTRL		Reset Control Register
 *   0x38  -- RSTSTAT		Reset Status Register
 *   0x60  -- GPIOMODE		GPIO Mode Control Register
 */
#define RT2880_SYS_CNTL_BASE    (RALINK_SYSCTL_BASE)
#define RT2880_CHIP_REV_ID_REG  (RT2880_SYS_CNTL_BASE+0x0c)
#define RT2880_SYSCFG_REG       (RT2880_SYS_CNTL_BASE+0x10)
#define RT2880_SYSCFG1_REG      (RT2880_SYS_CNTL_BASE+0x14)
#define RT2880_CLKCFG1_REG      (RT2880_SYS_CNTL_BASE+0x30)
#define RT2880_RSTCTRL_REG      (RT2880_SYS_CNTL_BASE+0x34)
#define RT2880_RSTSTAT_REG      (RT2880_SYS_CNTL_BASE+0x38)
#define RT2880_SYSCLKCFG_REG    (RT2880_SYS_CNTL_BASE+0x3c)
#define RT2880_GPIOMODE_REG     (RT2880_SYS_CNTL_BASE+0x60)

#define RT2880_PRGIO_ADDR       (RALINK_SYSCTL_BASE + 0x600) // Programmable I/O
#define RT2880_REG_PIOINT       (RT2880_PRGIO_ADDR + 0x90)
#define RT2880_REG_PIOEDGE      (RT2880_PRGIO_ADDR + 0xA0)
#define RT2880_REG_PIORENA      (RT2880_PRGIO_ADDR + 0x50)
#define RT2880_REG_PIOFENA      (RT2880_PRGIO_ADDR + 0x60)
#define RT2880_REG_PIODATA      (RT2880_PRGIO_ADDR + 0x20)
#define RT2880_REG_PIODIR       (RT2880_PRGIO_ADDR + 0x00)
#define RT2880_REG_PIOSET       (RT2880_PRGIO_ADDR + 0x30)
#define RT2880_REG_PIORESET     (RT2880_PRGIO_ADDR + 0x40)

#define RALINK_REG(x)		(*((volatile unsigned long *)(x)))

#define ra_inb(offset)		(*(volatile unsigned char *)(offset))
#define ra_inw(offset)		(*(volatile unsigned short *)(offset))
#define ra_inl(offset)		(*(volatile unsigned long *)(offset))

#define ra_outb(offset,val)	(*(volatile unsigned char *)(offset) = val)
#define ra_outw(offset,val)	(*(volatile unsigned short *)(offset) = val)
#define ra_outl(offset,val)	(*(volatile unsigned long *)(offset) = val)

#define ra_and(addr, value) ra_outl(addr, (ra_inl(addr) & (value)))
#define ra_or(addr, value) ra_outl(addr, (ra_inl(addr) | (value)))

#define RT2880_WDRST            (1U<<1)
#define RT2880_SWSYSRST         (1U<<2)
#define RT2880_SWCPURST         (1U<<3)
#define RT2880_WDT2SYSRST_EN    (1U<<31) /* WDT Reset applly to System Reset */

#define RT2880_UPHY0_CLK_EN     (1<<18)
#define RT2880_UPHY1_CLK_EN     (1<<20)

/*
* for USB
*/
#if defined (RALINK_USB) || defined (MTK_USB)
#ifdef CONFIG_RALINK_MT7621
#define CONFIG_USB_STORAGE    1
#define CONFIG_DOS_PARTITION	1
#define LITTLEENDIAN
#define CONFIG_CRC32_VERIFY
#define CONFIG_SYS_USB_XHCI_MAX_ROOT_PORTS	3
#else
#define CONFIG_USB_OHCI		1
#define CONFIG_SYS_USB_OHCI_MAX_ROOT_PORTS	2
#define CONFIG_SYS_USB_OHCI_REGS_BASE		0x101C1000
#define CONFIG_SYS_USB_OHCI_SLOT_NAME		"rt3680"
#define CONFIG_USB_EHCI		1
#define CONFIG_USB_STORAGE    1
#define CONFIG_DOS_PARTITION	1
#define LITTLEENDIAN
#define CONFIG_CRC32_VERIFY
#endif
#endif /* RALINK_USB */

/* Ethernet related */
#define CONFIG_NET_MULTI	1
#define milisecdelay(_x)			udelay((_x) * 1000)
#define milisecdelay(_x)			udelay((_x) * 1000)
//#define USE_PIO_DBG		1

#endif	/* __CONFIG_H */
