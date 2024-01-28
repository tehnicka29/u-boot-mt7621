/*
 * (C) Copyright 2000-2004
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#ifndef __COMMON_H_
#define __COMMON_H_	1

#undef	_LINUX_CONFIG_H
#define _LINUX_CONFIG_H 1	/* avoid reading Linux autoconf.h file	*/

typedef unsigned char		uchar;

#define XMK_STR(x)	#x
#define MK_STR(x)	XMK_STR(x)

#include <config.h>
#include <asm-mips/byteorder.h>
#include <linux/types.h>
#include <linux/string.h>
#include <stdarg.h>

#ifndef __FUNCTION__
#define __FUNCTION__ __func__
#endif

#ifdef CONFIG_ARM
#define asmlinkage	/* nothing */
#endif

#include <part.h>
#include <image.h>

#ifdef	DEBUG
#define debug(fmt,args...)	printf (fmt ,##args)
#define debugX(level,fmt,args...) if (DEBUG>=level) printf(fmt,##args);
#else
#define debug(fmt,args...)
#define debugX(level,fmt,args...)
#endif	/* DEBUG */

#ifndef BUG
#define BUG() do { \
        printf("BUG: failure at %s:%d/%s()!\n", __FILE__, __LINE__, __FUNCTION__); \
} while (0)
#define BUG_ON(condition) do { if (unlikely((condition)!=0)) BUG(); } while(0)
#endif /* BUG */

#define PAD_COUNT(s, pad) (((s) - 1) / (pad) + 1)
#define PAD_SIZE(s, pad) (PAD_COUNT(s, pad) * pad)
#define ALLOC_ALIGN_BUFFER_PAD(type, name, size, align, pad)            \
        char __##name[ROUND(PAD_SIZE((size) * sizeof(type), pad), align)  \
                      + (align - 1)];                                   \
                                                                        \
        type *name = (type *) ALIGN((uintptr_t)__##name, align)
#define ALLOC_ALIGN_BUFFER(type, name, size, align)             \
        ALLOC_ALIGN_BUFFER_PAD(type, name, size, align, 1)
#define ALLOC_CACHE_ALIGN_BUFFER_PAD(type, name, size, pad)             \
        ALLOC_ALIGN_BUFFER_PAD(type, name, size, ARCH_DMA_MINALIGN, pad)
#define ALLOC_CACHE_ALIGN_BUFFER(type, name, size)                      \
        ALLOC_ALIGN_BUFFER(type, name, size, ARCH_DMA_MINALIGN)


typedef void (interrupt_handler_t)(void *);

#include <asm/u-boot.h> /* boot information for Linux kernel */
#include <asm/global_data.h>	/* global data used for startup functions */
/*
 * General Purpose Utilities
 */
#define min(X, Y)				\
	({ typeof (X) __x = (X), __y = (Y);	\
		(__x < __y) ? __x : __y; })

#define max(X, Y)				\
	({ typeof (X) __x = (X), __y = (Y);	\
		(__x > __y) ? __x : __y; })

#define NUM_RX_DESC 24
#define NUM_TX_DESC 24


//   For loopback test

typedef struct _BUFFER_ELEM_    BUFFER_ELEM;

struct _BUFFER_ELEM_
{
	int tx_idx;
    unsigned char *pbuf;
    BUFFER_ELEM       *next;
    
    
};

typedef struct _VALID_BUFFER_STRUCT_    VALID_BUFFER_STRUCT;

struct _VALID_BUFFER_STRUCT_
{
    BUFFER_ELEM    *head;
    BUFFER_ELEM    *tail;
};

/*
 * Function Prototypes
 */

void	hang		(void) __attribute__ ((noreturn));

/* */
long int initdram (int);
int	display_options (void);
void	print_size (ulong, const char *);

/* common/main.c */
void	main_loop	(void);
int	run_command	(const char *cmd, int flag);
int	readline	(const char *const prompt, int show_buf);
void	init_cmd_timeout(void);
void	reset_cmd_timeout(void);

/* lib_$(ARCH)/board.c */
void __attribute__((nomips16)) board_init_f(ulong);
void __attribute__((nomips16)) board_init_r(gd_t *, ulong);
int	checkboard    (void);
int	checkflash    (void);
int	checkdram     (void);
char *	strmhz(char *buf, long hz);
int	last_stage_init(void);
extern ulong monitor_flash_len;

/* common/flash.c */
void flash_perror (int);

/* common/cmd_autoscript.c */
int	autoscript (ulong addr);

/* common/cmd_bootm.c */
void	print_image_hdr (image_header_t *hdr);

extern ulong load_addr;		/* Default Load Address */
extern image_header_t header;
extern int verify_kernel_image(ulong, ulong *, ulong *, ulong *, int);
extern int verify_uboot_image(const u32 *data, unsigned int length);

int rootfs_func(int check);

/* common/cmd_nvedit.c */
int env_init (void);
void env_relocate (void);
char *getenv (uchar *);
int getenv_r (uchar *name, uchar *buf, unsigned len);
int saveenv (void);
void setenv (char *, char *);

#ifdef CONFIG_AUTO_COMPLETE
int env_complete(char *var, int maxv, char *cmdv[], int maxsz, char *buf);
#endif

/* common/exports.c */
void	jumptable_init(void);

/* common/memsize.c */
//int	get_ram_size  (volatile long *, long);

/* $(BOARD)/$(BOARD).c */
void	reset_phy     (void);
void	fdc_hw_init   (void);

/* $(BOARD)/eeprom.c */
void eeprom_init  (void);
#ifndef CONFIG_SPI
int  eeprom_probe (unsigned dev_addr, unsigned offset);
#endif
int  eeprom_read  (unsigned dev_addr, unsigned offset, uchar *buffer, unsigned cnt);
int  eeprom_write (unsigned dev_addr, unsigned offset, uchar *buffer, unsigned cnt);
#ifdef CONFIG_LWMON
extern uchar pic_read  (uchar reg);
extern void  pic_write (uchar reg, uchar val);
#endif

/*
 * Set this up regardless of board
 * type, to prevent errors.
 */
#if defined(CONFIG_SPI) || !defined(CFG_I2C_EEPROM_ADDR)
# define CFG_DEF_EEPROM_ADDR 0
#else
# define CFG_DEF_EEPROM_ADDR CFG_I2C_EEPROM_ADDR
#endif /* CONFIG_SPI || !defined(CFG_I2C_EEPROM_ADDR) */

#if defined(CONFIG_SPI)
extern void spi_init_f (void);
extern void spi_init_r (void);
extern ssize_t spi_read	 (uchar *, int, uchar *, int);
extern ssize_t spi_write (uchar *, int, uchar *, int);
#endif

#if defined(CFG_DRAM_TEST)
int testdram(void);
#endif /* CFG_DRAM_TEST */

/* $(CPU)/start.S */
void __attribute__((nomips16)) relocate_code(ulong, gd_t *, ulong);

/* $(CPU)/cpu.c */
ulong	get_tbclk(void);

/* $(CPU)/serial.c */
int serial_init(void);
void serial_putc(const char);
void serial_puts(const char *);
int	serial_getc(void);
int	serial_tstc(void);

/* $(CPU)/speed.c */
int	get_clocks (void);
ulong	get_bus_freq  (ulong);

/* $(CPU)/interrupts.c */
void	reset_timer(void);
__attribute__((nomips16)) unsigned long get_timer(unsigned long base);
void	set_timer(unsigned long t);

/* lib_$(ARCH)/cache.c */
__attribute__((nomips16)) void	flush_cache   (unsigned long, unsigned long);

/* lib_$(ARCH)/ticks.S */
unsigned long long get_ticks(void);
void	wait_ticks    (unsigned long);

/* lib_$(ARCH)/time.c */
__attribute__((nomips16)) void	udelay	      (unsigned long);
#define milisecdelay(_x)                        udelay((_x) * 1000)
__attribute__((nomips16)) void	mdelay	      (unsigned long);
ulong	usec2ticks    (unsigned long usec);
ulong	ticks2usec    (unsigned long ticks);
int	init_timebase (void);

/* lib_generic/vsprintf.c */
ulong	simple_strtoul(const char *cp,char **endp,unsigned int base);
#ifdef CFG_64BIT_VSPRINTF
unsigned long long	simple_strtoull(const char *cp,char **endp,unsigned int base);
#endif
long	simple_strtol(const char *cp,char **endp,unsigned int base);
void	panic(const char *fmt, ...);
int	sprintf(char * buf, const char *fmt, ...);
int	vsprintf(char *buf, const char *fmt, va_list args);

/* lib_generic/crc32.c */
ulong crc32 (ulong, const unsigned char *, uint);
ulong crc32_no_comp (ulong, const unsigned char *, uint);

/* common/console.c */
int	console_init_f(void);	/* Before relocation; uses the serial  stuff	*/
int	console_init_r(void);	/* After  relocation; uses the console stuff	*/
int	console_assign (int file, char *devname);	/* Assign the console	*/
int	ctrlc (void);
int	had_ctrlc (void);	/* have we had a Control-C since last clear? */
void	clear_ctrlc (void);	/* clear the Control-C condition */
int	disable_ctrlc (int);	/* 1 to disable, 0 to enable Control-C detect */

/*
 * STDIO based functions (can always be used)
 */

/* serial stuff */
void	serial_printf (const char *fmt, ...);

/* stdin */
int	getc(void);
int	tstc(void);

/* stdout */
void	putc(const char c);
void	puts(const char *s);
void	printf(const char *fmt, ...);
void	vprintf(const char *fmt, va_list args);

/* stderr */
#define eputc(c)		fputc(stderr, c)
#define eputs(s)		fputs(stderr, s)
#define eprintf(fmt,args...)	fprintf(stderr,fmt ,##args)

/*
 * FILE based functions (can only be used AFTER relocation!)
 */

#define stdin		0
#define stdout		1
#define stderr		2
#define MAX_FILES	3

void	fprintf(int file, const char *fmt, ...);
void	fputs(int file, const char *s);
void	fputc(int file, const char c);
int	ftstc(int file);
int	fgetc(int file);

#ifdef CONFIG_SHOW_BOOT_PROGRESS
void	show_boot_progress (int status);
#endif

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#define ROUND(a,b)		(((a) + (b) - 1) & ~((b) - 1))
#define DIV_ROUND(n,d)		(((n) + ((d)/2)) / (d))
#define DIV_ROUND_UP(n,d)	(((n) + (d) - 1) / (d))
#define roundup(x, y)		((((x) + ((y) - 1)) / (y)) * (y))

/*
 * Divide positive or negative dividend by positive divisor and round
 * to closest integer. Result is undefined for negative divisors and
 * for negative dividends if the divisor variable type is unsigned.
 */
#define DIV_ROUND_CLOSEST(x, divisor)(			\
{							\
	typeof(x) __x = x;				\
	typeof(divisor) __d = divisor;			\
	(((typeof(x))-1) > 0 ||				\
         ((typeof(divisor))-1) > 0 || (__x) > 0) ?	\
		(((__x) + ((__d) / 2)) / (__d)) :	\
		(((__x) - ((__d) / 2)) / (__d));	\
}							\
)

#define ALIGN(x,a)		__ALIGN_MASK((x),(typeof(x))(a)-1)
#define __ALIGN_MASK(x,mask)	(((x)+(mask))&~(mask))

#endif	/* __COMMON_H_ */
