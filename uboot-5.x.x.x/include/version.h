/*
 * (C) Copyright 2000-2003
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

#ifndef	__VERSION_H__
#define	__VERSION_H__

#include "../uboot_version.h"
#include <configs/rt2880.h>

#define U_BOOT_VERSION	"U-Boot 1.1.3"

#define CHIP_TYPE		"ASIC"

#define CHIP_VERSION	"7621_MP"

#define GMAC_MODE		"(MAC to MT7530 Mode)"

#if defined (ON_BOARD_16M_DRAM_COMPONENT)
#define DRAM_COMPONENT  16
#elif defined (ON_BOARD_64M_DRAM_COMPONENT)
#define DRAM_COMPONENT	64
#elif defined (ON_BOARD_128M_DRAM_COMPONENT)
#define DRAM_COMPONENT	128
#elif defined (ON_BOARD_256M_DRAM_COMPONENT)
#define DRAM_COMPONENT	256
#elif defined (ON_BOARD_512M_DRAM_COMPONENT)
#define DRAM_COMPONENT	512
#elif defined (ON_BOARD_1024M_DRAM_COMPONENT)
#define DRAM_COMPONENT	1024
#elif defined (ON_BOARD_1024M_KGD_DRAM_COMPONENT)
#define DRAM_COMPONENT	1024
#elif defined (ON_BOARD_2048M_DRAM_COMPONENT)
#define DRAM_COMPONENT  2048
#elif defined (ON_BOARD_4096M_DRAM_COMPONENT)
#define DRAM_COMPONENT  3584
#elif defined CFG_ENV_IS_IN_SPI
#define DRAM_COMPONENT	({ int _x = ((RALINK_REG(RT2880_SYSCFG_REG) >> 26) & 0x3); \
		(_x == 0x2)? 256 : (_x == 0x1)? 128 : 64; })
#else
#error "DRAM SIZE not defined"
#endif

#if defined (ON_BOARD_16BIT_DRAM_BUS)
#define DRAM_BUS	16
#elif defined (ON_BOARD_32BIT_DRAM_BUS)
#define DRAM_BUS	32
#elif defined (CFG_ENV_IS_IN_SPI)
#define DRAM_BUS	({ ((RALINK_REG(RT2880_SYSCFG_REG) >> 28) & 0x1)? 32 : 16; })
#else
#error "DRAM BUS not defined"
#endif

#ifdef ON_BOARD_DDR_WIDTH_8 
#define DDR_INFO "DDR, width 8"
#else
#define DDR_INFO "DDR, width 16"
#endif

#define DRAM_SIZE ((DRAM_COMPONENT/8)*(DRAM_BUS/16))

#if defined (ON_BOARD_2M_FLASH_COMPONENT)
#define FLASH_MSG "Flash component: 2 MBytes NOR Flash"
#elif defined (ON_BOARD_4M_FLASH_COMPONENT)
#define FLASH_MSG "Flash component: 4 MBytes NOR Flash"
#elif defined (ON_BOARD_8M_FLASH_COMPONENT)
#define FLASH_MSG "Flash component: 8 MBytes NOR Flash"
#elif defined (ON_BOARD_16M_FLASH_COMPONENT)
#define FLASH_MSG "Flash component: 16 MBytes NOR Flash"
#elif defined (ON_BOARD_32M_FLASH_COMPONENT)
#define FLASH_MSG "Flash component: 32 MBytes NOR Flash"
  #ifndef RT3052_MP2
  #error "32MB flash is only supported by RT3052 MP2 currently"
  #endif
#endif

#define RLT_MTK_VENDOR_NAME	"MediaTek"

#define SHOW_VER_STR()	\
	do {	\
		printf("============================================ \n"); \
		printf("%s U-Boot Version: %s\n", RLT_MTK_VENDOR_NAME, RALINK_LOCAL_VERSION); \
		printf("-------------------------------------------- \n"); \
		printf("%s %s %s\n",CHIP_TYPE, CHIP_VERSION, GMAC_MODE); \
		printf("DRAM component: %d Mbits %s\n", DRAM_COMPONENT, DDR_INFO); \
		printf("DRAM bus: %d bit\n", DRAM_BUS); \
		printf("Total memory: %d MBytes\n", DRAM_SIZE); \
		printf("%s\n", FLASH_MSG); \
		printf("%s\n", "Date:" __DATE__ "  Time:" __TIME__ ); \
		printf("============================================ \n"); \
	}while(0)

#endif	/* __VERSION_H__ */
