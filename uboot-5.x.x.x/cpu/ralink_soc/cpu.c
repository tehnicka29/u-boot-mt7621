/*
 * (C) Copyright 2003
 * Wolfgang Denk, DENX Software Engineering, <wd@denx.de>
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

#include <common.h>
#include <command.h>
#include <asm-mips/mipsregs.h>
#include <asm-mips/cacheops.h>
#include <rt_mmap.h>
#include <configs/rt2880.h>

#define cache_op(op,addr)						\
	 __asm__ __volatile__(						\
	"       .set    push                                    \n"	\
	"       .set    noreorder                               \n"	\
	"       .set    mips3\n\t                               \n"	\
	"       cache   %0, %1                                  \n"	\
	"       .set    pop                                     \n"	\
	:								\
	: "i" (op), "R" (*(unsigned char *)(addr)))

int do_reset(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	RALINK_REG(RALINK_RSTCTRL_REG) = RALINK_SYS_RST;
	return 1;
}

void watchdog_reset(void)
{
	/*ulong reg;*/
	//debug("resetting watchdog...\n");
	RALINK_REG(RALINK_TGLB_REG) |= (1U << 9); /*WDTRST*/
	RALINK_REG(RALINK_TGLB_REG) &= ~(1U << 9); /*WDTRST*/
#if 0
	reg = RALINK_REG(RALINK_WDT_REG) & 0xFFFF;
	printf("value read: %u\n", reg);
#endif
}

void watchdog_set(ulong sec) {
	ulong reg, limit, prescale;

	RALINK_REG(RALINK_WDTCTL_REG) &= ~(1U << 7); /*WDTEN*/

	if (sec == 0) {
		RALINK_REG(RT2880_RSTSTAT_REG) &= ~(RT2880_WDRST | RT2880_SWSYSRST | RT2880_SWCPURST | RT2880_WDT2SYSRST_EN);
		return;
	}

	/* we use fixed prescale of 50000 (50ms) */
	prescale = 50000;

	/* prevent overflow */
	if (sec > 3200) {
		//debug("wdt clamp seconds from %u to %u\n", sec, 3200);
		sec = 3200;
	}

	limit = sec * 1000 / 50;

	//if (limit > 65535) {
	//	debug("wdt clamp limit from %u to %u\n", limit, 65535);
	//	limit = 65535;
	//}
	// debug("wdt value after stop=%u\n", RALINK_REG(RALINK_WDT_REG) & 0xFFFF);
	// debug("setting prescale=%u, limit=%u\n", prescale, limit);
	
	reg = RALINK_REG(RALINK_WDTLMT_REG);
	reg = reg & ~0xFFFFU;
	reg |= (limit & 0xFFFFU); /*WDTLMT*/
	RALINK_REG(RALINK_WDTLMT_REG) = reg;
	
	reg = RALINK_REG(RALINK_WDTCTL_REG);
	reg = reg & ~(0xFFFFU << 16);
	reg = reg | ((prescale & 0xFFFFU) << 16) /*WDTPRES*/ | (1U << 7) /*WDTEN*/ | (1U << 4) /*WDTAL*/;
	RALINK_REG(RALINK_WDTCTL_REG) = reg;
	
	RALINK_REG(RT2880_RSTSTAT_REG) |= RT2880_WDT2SYSRST_EN;
	
	//reg = RALINK_REG(RALINK_WDT_REG) & 0xFFFF;
	//printf("wdt value on start=%u\n", reg);
}

#ifdef CONFIG_SYS_CACHELINE_SIZE

static inline unsigned long icache_line_size(void)
{
	return CONFIG_SYS_CACHELINE_SIZE;
}

static inline unsigned long dcache_line_size(void)
{
	return CONFIG_SYS_CACHELINE_SIZE;
}

#else /* !CONFIG_SYS_CACHELINE_SIZE */

__attribute__((nomips16)) static inline unsigned long icache_line_size(void)
{
	unsigned long conf1, il;
	conf1 = read_c0_config1();
	il = (conf1 & MIPS_CONF1_IL) >> MIPS_CONF1_IL_SHIFT;
	if (!il)
		return 0;
	return 2 << il;
}

__attribute__((nomips16)) static inline unsigned long dcache_line_size(void)
{
	unsigned long conf1, dl;
	conf1 = read_c0_config1();
	dl = (conf1 & MIPS_CONF1_DL) >> MIPS_CONF1_DL_SHIFT;
	if (!dl)
		return 0;
	return 2 << dl;
}

#endif /* !CONFIG_SYS_CACHELINE_SIZE */

__attribute__((nomips16)) void flush_cache (ulong start_addr, ulong size)
{
//#if defined (CONFIG_USB_XHCI)
	unsigned long ilsize = icache_line_size();
	unsigned long dlsize = dcache_line_size();
	unsigned long addr, aend;

	/* aend will be miscalculated when size is zero, so we return here */
	if (size == 0)
		return;

	addr = start_addr & ~(dlsize - 1);
	aend = (start_addr + size - 1) & ~(dlsize - 1);

	if (ilsize == dlsize) {
		/* flush I-cache & D-cache simultaneously */
		while (1) {
			cache_op(HIT_WRITEBACK_INV_D, addr);
			cache_op(HIT_INVALIDATE_I, addr);
			if (addr == aend)
				break;
			addr += dlsize;
		}
		return;
	}

	/* flush D-cache */
	while (1) {
		cache_op(HIT_WRITEBACK_INV_D, addr);
		if (addr == aend)
			break;
		addr += dlsize;
	}

	/* flush I-cache */
	addr = start_addr & ~(ilsize - 1);
	aend = (start_addr + size - 1) & ~(ilsize - 1);
	while (1) {
		cache_op(HIT_INVALIDATE_I, addr);
		if (addr == aend)
			break;
		addr += ilsize;
	}
//#endif
}

#if defined (CONFIG_USB_XHCI)
__attribute__( (nomips16) ) void flush_dcache_range( ulong start_addr, ulong stop )
{
	unsigned long lsize = dcache_line_size();
	unsigned long addr = start_addr & ~(lsize - 1);
	unsigned long aend = (stop - 1) & ~(lsize - 1);

	while ( 1 ) {
		cache_op( HIT_WRITEBACK_INV_D, addr );
		if ( addr == aend )
			break;
		addr += lsize;
	}
}

__attribute__( (nomips16) ) void invalidate_dcache_range( ulong start_addr, ulong stop )
{
	unsigned long lsize = dcache_line_size();
	unsigned long addr = start_addr & ~(lsize - 1);
	unsigned long aend = (stop - 1) & ~(lsize - 1);

	while ( 1 ) {
		cache_op( HIT_INVALIDATE_D, addr );
		if ( addr == aend )
			break;
		addr += lsize;
	}
}
#endif

#ifdef RT2880_U_BOOT_CMD_OPEN
#if 0
__attribute__((nomips16)) void write_one_tlb( int index, u32 pagemask, u32 hi, u32 low0, u32 low1 ){
	write_32bit_cp0_register(CP0_ENTRYLO0, low0);
	write_32bit_cp0_register(CP0_PAGEMASK, pagemask);
	write_32bit_cp0_register(CP0_ENTRYLO1, low1);
	write_32bit_cp0_register(CP0_ENTRYHI, hi);
	write_32bit_cp0_register(CP0_INDEX, index);
	tlb_write_indexed();
}
#endif
#endif
