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

#include <common.h>
#include <command.h>
#include <malloc.h>
#include <devices.h>
#include <version.h>
#include <net.h>
#include <environment.h>
#include <asm/mipsregs.h>
#include <rt_mmap.h>
#include <spi_api.h>
#include <watchdog.h>

#include <gpio.h>
#include <cmd_tftpServer.h>

DECLARE_GLOBAL_DATA_PTR;
#undef DEBUG

int modifies= 0;
unsigned char BootType;

#ifdef DEBUG
   #define DATE      "05/25/2006"
   #define VERSION   "v0.00e04"
#endif
#if ( ((CFG_ENV_ADDR+CFG_ENV_SIZE) < CFG_MONITOR_BASE) || \
      (CFG_ENV_ADDR >= (CFG_MONITOR_BASE + CFG_MONITOR_LEN)) ) || \
    defined(CFG_ENV_IS_IN_NVRAM)
#define	TOTAL_MALLOC_LEN	(CFG_MALLOC_LEN + CFG_ENV_SIZE)
#else
#define	TOTAL_MALLOC_LEN	CFG_MALLOC_LEN
#endif
#define ARGV_LEN  128

extern int timer_init(void);

extern void  rt2880_eth_halt(struct eth_device* dev);

extern void setup_internal_gsw(void); 
//extern void pci_init(void);

extern int incaip_set_cpuclk(void);
extern int do_tftpb (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[]);
#ifdef RALINK_UPGRADE_BY_SERIAL
extern int do_load_serial_bin(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[]);
#endif
extern int flash_sect_protect (int p, ulong addr_first, ulong addr_last);
int flash_sect_erase (ulong addr_first, ulong addr_last);
extern int reset_to_default( void );
int get_addr_boundary (ulong *addr);
extern int do_reset(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[]);
extern void input_value(u8 *str);
extern void rt305x_esw_init(void);
extern void LANWANPartition(void);

extern struct eth_device* 	rt2880_pdev;

extern ulong uboot_end_data;
extern ulong uboot_end;

#if defined (RALINK_USB ) || defined (MTK_USB)
#include <usb.h>
extern int usb_stor_curr_dev;
#endif

ulong monitor_flash_len;

const char version_string[] =
	U_BOOT_VERSION" (" __DATE__ " - " __TIME__ ")";

unsigned long mips_cpu_feq;
unsigned long mips_bus_feq;

/*
 * Begin and End of memory area for malloc(), and current "brk"
 */
static ulong mem_malloc_start;
static ulong mem_malloc_end;
static ulong mem_malloc_brk;

static char  file_name_space[ARGV_LEN];

#define read_32bit_cp0_register_with_select1(source)            \
({ int __res;                                                   \
        __asm__ __volatile__(                                   \
        ".set\tpush\n\t"                                        \
        ".set\treorder\n\t"                                     \
        "mfc0\t%0,"STR(source)",1\n\t"                          \
        ".set\tpop"                                             \
        : "=r" (__res));                                        \
        __res;})

#if defined (CONFIG_DDR_CAL)
__attribute__((nomips16)) void dram_cali(void);
#endif

static void Init_System_Mode(void)
{
	u32 reg;
	u8	clk_sel;

	reg = RALINK_REG(RALINK_SYSCFG_REG);
		
	/* 
	 * CPU_CLK_SEL (bit 21:20)
	 */
	clk_sel = (reg>>5)&0x1; // OCP
	reg = RALINK_REG(RALINK_CLKCFG0_REG);
	if( reg & ((0x1UL) << 30)) {
		reg = RALINK_REG(RALINK_MEMCTRL_BASE + 0x648);
		mips_cpu_feq = (((reg >> 4) & 0x7F) + 1) * 1000 * 1000;
		reg = RALINK_REG(RALINK_SYSCFG_REG);
		reg = (reg >> 6) & 0x7;
		if(reg >= 6) { //25Mhz Xtal
			mips_cpu_feq = mips_cpu_feq * 25;
		} else if (reg >=3) { //40Mhz Xtal
			mips_cpu_feq = mips_cpu_feq * 20;
		} else { //20Mhz Xtal
			/* TODO */
		}
	}else {
		reg = RALINK_REG(RALINK_CUR_CLK_STS_REG);
		mips_cpu_feq = (500 * (reg & 0x1F) / ((reg >> 8) & 0x1F)) * 1000 * 1000;
	}
	/* SYS_CLK always CPU/4 (not depend from OCP) */
	mips_bus_feq = mips_cpu_feq/4;

   	//RALINK_REG(RT2880_SYSCFG_REG) = reg;
}


/*
 * The Malloc area is immediately below the monitor copy in DRAM
 */
static void mem_malloc_init (void)
{

	ulong dest_addr = CFG_MONITOR_BASE + gd->reloc_off;

	mem_malloc_end = dest_addr;
	mem_malloc_start = dest_addr - TOTAL_MALLOC_LEN;
	mem_malloc_brk = mem_malloc_start;

	memset ((void *) mem_malloc_start,
		0,
		mem_malloc_end - mem_malloc_start);
}

void *sbrk (ptrdiff_t increment)
{
	ulong old = mem_malloc_brk;
	ulong new = old + increment;

	if ((new < mem_malloc_start) || (new > mem_malloc_end)) {
		return (NULL);
	}
	mem_malloc_brk = new;
	return ((void *) old);
}

static int init_func_ram (void)
{

#ifdef	CONFIG_BOARD_TYPES
	int board_type = gd->board_type;
#else
	int board_type = 0;	/* use dummy arg */
#endif
	puts ("DRAM: ");

	if ((gd->ram_size = initdram (board_type)) > 0) {
		ulong ram_size = gd->ram_size;
#if defined (ON_BOARD_4096M_DRAM_COMPONENT)
		ram_size += 64*1024*1024;
#endif
		print_size (ram_size, "\n");
		return (0);
	}
	puts ("*** failed ***\n");

	return (1);
}

static int display_banner(void)
{
   
	printf ("\n\n%s\n\n", version_string);
	return (0);
}

/*
static void display_flash_config(ulong size)
{
	puts ("Flash: ");
	print_size (size, "\n");
}
*/

static int init_baudrate (void)
{
	//uchar tmp[64]; /* long enough for environment variables */
	//int i = getenv_r ("baudrate", tmp, sizeof (tmp));
	//kaiker 
	gd->baudrate = CONFIG_BAUDRATE;
/*
	gd->baudrate = (i > 0)
			? (int) simple_strtoul (tmp, NULL, 10)
			: CONFIG_BAUDRATE;
*/
	return (0);
}


/*
 * Breath some life into the board...
 *
 * The first part of initialization is running from Flash memory;
 * its main purpose is to initialize the RAM so that we
 * can relocate the monitor code to RAM.
 */

void __attribute__((nomips16)) board_init_f(ulong bootflag)
{
	gd_t gd_data, *id;
	bd_t *bd;  
	//init_fnc_t **init_fnc_ptr;
	ulong addr, addr_sp, len = (ulong)&uboot_end - CFG_MONITOR_BASE;
	ulong *s;
	u32 value;
	void (*ptr)(void);
	u32 fdiv = 0, step = 0, frac = 0, i;

	value = le32_to_cpu(RALINK_REG(RALINK_SPI_BASE + 0x3c));
	value &= ~(0xFFF);
	value |= 5; //work-around 3-wire SPI issue (3 for RFB, 5 for EVB)
	RALINK_REG(RALINK_SPI_BASE + 0x3c) = cpu_to_le32(value);

#if (MT7621_CPU_FREQUENCY!=50)
	value = RALINK_REG(RALINK_CUR_CLK_STS_REG);
	/*	4:0   - CUR_CPU_FFRAC
		12:8  - CUR_CPU_FDIV
		18:16 - CUR_OCP_RATIO
		20:20 - SAME_FREQ (SYS_CLK = DRAM_clk)
	*/
	fdiv = ((value>>8)&0x1F);
	frac = (unsigned long)(value&0x1F);

	i = 0;

	while(frac < fdiv) {
		value = RALINK_REG(RALINK_DYN_CFG0_REG);
		fdiv = ((value>>8)&0x0F);
		fdiv--;
		value &= ~(0x0F<<8);
		value |= (fdiv<<8);
		RALINK_REG(RALINK_DYN_CFG0_REG) = value;
		udelay(500);
		i++;
		value = RALINK_REG(RALINK_CUR_CLK_STS_REG);
		fdiv = ((value>>8)&0x1F);
		frac = (unsigned long)(value&0x1F);
	}
#endif /* MT7621_CPU_FREQUENCY!=50 */

#if ((MT7621_CPU_FREQUENCY!=50) && (MT7621_CPU_FREQUENCY!=500))
	//change CPLL from GPLL to MEMPLL
	value = RALINK_REG(RALINK_CLKCFG0_REG);
	/* 31:30 CPU_CLK_SEL
		 3 - XTAL clock
		 2 - XTAL clock
		 1 - CPU PLL
		 0 - 500MHz
	*/
	value &= ~(0x3<<30);
	value |= (0x1<<30);
	RALINK_REG(RALINK_CLKCFG0_REG) = value;
#endif

	/* Pointer is writable since we allocated a register for it.
	 */
	gd = &gd_data;

	/* compiler optimization barrier needed for GCC >= 3.4 */
	__asm__ __volatile__("": : :"memory");

	memset ((void *)gd, 0, sizeof (gd_t));

//#if defined (RT6855A_ASIC_BOARD) || defined(RT6855A_FPGA_BOARD)
	watchdog_reset();
//#endif
	timer_init();
	env_init();			/* initialize environment */
	init_baudrate();	/* initialze baudrate settings */
	serial_init();		/* serial communications setup */
	console_init_f();
//#if (TEXT_BASE == 0xBFC00000) || (TEXT_BASE == 0xBF000000) || (TEXT_BASE == 0xBC000000)
#if defined (CONFIG_DDR_CAL)
	ptr = dram_cali;
	ptr = (void*)((u32)ptr & ~(1<<29));
	(*ptr)();
#endif
//#endif
	display_banner();		/* say that we are here */
	checkboard();
	init_func_ram();

	debug("gpio_init()\n");
	gpio_init();

	debug("Reset Frame Engine...");
	/* reset Frame Engine */
	value = le32_to_cpu(RALINK_REG(RALINK_RSTCTRL_REG));
	/* reset PPE & FE */
	value |= (RALINK_FE_RST | RALINK_PPE_RST);
	//value |= (RALINK_PCIE0_RST | RALINK_PCIE1_RST | RALINK_PCIE2_RST); /* PCIE de-assert for E3 */
	RALINK_REG(RALINK_RSTCTRL_REG) = cpu_to_le32(value);
	udelay(100);
	value &= ~(RALINK_FE_RST | RALINK_PPE_RST);
	RALINK_REG(RALINK_RSTCTRL_REG) = cpu_to_le32(value);
	udelay(300);

	debug("OK\n");

#ifdef DEBUG
	debug("rt2880 uboot %s %s\n", VERSION, DATE);
#endif
	/*
	 * Now that we have DRAM mapped and working, we can
	 * relocate the code and continue running from DRAM.
	 */
	addr = CFG_SDRAM_BASE + gd->ram_size;

	/* We can reserve some RAM "on top" here.
	 */
#ifdef DEBUG
	debug ("SERIAL_CLOCK_DIVISOR =%d \n", SERIAL_CLOCK_DIVISOR);
	debug ("kaiker,,CONFIG_BAUDRATE =%d \n", CONFIG_BAUDRATE); 
	debug ("SDRAM SIZE:%08X\n",gd->ram_size);
#endif

	/* round down to next 4 kB limit.
	 */
	addr &= ~(4096 - 1);
//#ifdef DEBUG
	debug ("Top of RAM usable for U-Boot at: %08x\n", addr);
//#endif

	/* Reserve memory for U-Boot code, data & bss
	 * round down to next 16 kB limit
	 */
	addr -= len;
	addr &= ~(16 * 1024 - 1);
//#ifdef DEBUG
	debug ("Reserving %dk for U-Boot at: %08x\n", len >> 10, addr);
//#endif
	 /* Reserve memory for malloc() arena.
	 */
	addr_sp = addr - TOTAL_MALLOC_LEN;
//#ifdef DEBUG
	debug ("Reserving %dk for malloc() at: %08x\n",
			TOTAL_MALLOC_LEN >> 10, addr_sp);
//#endif
	/*
	 * (permanently) allocate a Board Info struct
	 * and a permanent copy of the "global" data
	 */
	addr_sp -= sizeof(bd_t);
	bd = (bd_t *)addr_sp;
	gd->bd = bd;
//#ifdef DEBUG
	debug ("Reserving %d Bytes for Board Info at: %08x\n",
			sizeof(bd_t), addr_sp);
//#endif
	addr_sp -= sizeof(gd_t);
	id = (gd_t *)addr_sp;
//#ifdef DEBUG
	debug ("Reserving %d Bytes for Global Data at: %08x\n",
			sizeof (gd_t), addr_sp);
//#endif
 	/* Reserve memory for boot params.
	 */
	addr_sp -= CFG_BOOTPARAMS_LEN;
	bd->bi_boot_params = addr_sp;
//#ifdef DEBUG
	debug ("Reserving %dk for boot params() at: %08x\n",
			CFG_BOOTPARAMS_LEN >> 10, addr_sp);
//#endif
	/*
	 * Finally, we set up a new (bigger) stack.
	 *
	 * Leave some safety gap for SP, force alignment on 16 byte boundary
	 * Clear initial stack frame
	 */
	addr_sp -= 16;
	addr_sp &= ~0xF;
	s = (ulong *)addr_sp;
	*s-- = 0;
	*s-- = 0;
	addr_sp = (ulong)s;
//#ifdef DEBUG
	debug ("Stack Pointer at: %08x\n", addr_sp);
//#endif

	/*
	 * Save local variables to board info struct
	 */
	bd->bi_memstart	= CFG_SDRAM_BASE;	/* start of  DRAM memory */
	bd->bi_memsize	= gd->ram_size;		/* size  of  DRAM memory in bytes */
	bd->bi_baudrate	= gd->baudrate;		/* Console Baudrate */

	memcpy (id, (void *)gd, sizeof (gd_t));

	debug ("relocate_code Pointer at: %08x\n", addr);
	relocate_code (addr_sp, id, addr);

	/* NOTREACHED - relocate_code() does not return */
}

#define SEL_LOAD_LINUX_WRITE_FLASH_BY_SERIAL 0
#define SEL_LOAD_LINUX_SDRAM            1
#define SEL_LOAD_LINUX_WRITE_FLASH      2
#define SEL_BOOT_FLASH                  3
#define SEL_ENTER_CLI                   4
#define SEL_LOAD_LINUX_WRITE_FLASH_BY_USB 5
#define SEL_LOAD_BOOT_WRITE_FLASH_BY_SERIAL 7
#define SEL_LOAD_BOOT_SDRAM             8
#define SEL_LOAD_BOOT_WRITE_FLASH       9


static void OperationSelectBanner(void)
{
	printf("\nPlease choose the operation: \n");
#ifdef RALINK_UPGRADE_BY_SERIAL
	printf("   %d: Load %s code then write to Flash via %s.\n", SEL_LOAD_LINUX_WRITE_FLASH_BY_SERIAL, "system", "Serial");
#endif
#ifdef TFTP_SUPPORT
	printf("   %d: Load system code to SDRAM via TFTP.\n", SEL_LOAD_LINUX_SDRAM);
	printf("   %d: Load %s code then write to Flash via %s.\n", SEL_LOAD_LINUX_WRITE_FLASH, "system", "TFTP");
#endif
	printf("   %d: Boot system code via Flash (default).\n", SEL_BOOT_FLASH);
#ifdef RALINK_CMDLINE
	printf("   %d: Enter boot command line interface.\n", SEL_ENTER_CLI);
#endif
#if defined (RALINK_USB) || defined (MTK_USB)
	printf("   %d: Load %s code then write to Flash via %s.\n", SEL_LOAD_LINUX_WRITE_FLASH_BY_USB, "system", "USB Storage");
#endif
#ifdef RALINK_UPGRADE_BY_SERIAL
	printf("   %d: Load %s code then write to Flash via %s.\n", SEL_LOAD_BOOT_WRITE_FLASH_BY_SERIAL, "U-Boot", "Serial");
#endif
#ifdef TFTP_SUPPORT
	printf("   %d: Load %s code then write to Flash via %s.\n", SEL_LOAD_BOOT_WRITE_FLASH, "U-Boot", "TFTP");
#endif
}

#ifdef TFTP_SUPPORT
int tftp_config(int type, char *argv[])
{
	char *s;
	char default_file[ARGV_LEN], file[ARGV_LEN], devip[ARGV_LEN], srvip[ARGV_LEN], default_ip[ARGV_LEN];

	printf(" Please Input new ones /or Ctrl-C to discard\n");

	memset(default_file, 0, ARGV_LEN);
	memset(file, 0, ARGV_LEN);
	memset(devip, 0, ARGV_LEN);
	memset(srvip, 0, ARGV_LEN);
	memset(default_ip, 0, ARGV_LEN);

	printf("\tInput device IP ");
	s = getenv("ipaddr");
	memcpy(devip, s, strlen(s));
	memcpy(default_ip, s, strlen(s));

	printf("(%s) ", devip);
	input_value(devip);
	setenv("ipaddr", devip);
	if (strcmp(default_ip, devip) != 0)
		modifies++;

	printf("\tInput server IP ");
	s = getenv("serverip");
	memcpy(srvip, s, strlen(s));
	memset(default_ip, 0, ARGV_LEN);
	memcpy(default_ip, s, strlen(s));

	printf("(%s) ", srvip);
	input_value(srvip);
	setenv("serverip", srvip);
	if (strcmp(default_ip, srvip) != 0)
		modifies++;

	if(type == SEL_LOAD_BOOT_SDRAM 
			|| type == SEL_LOAD_BOOT_WRITE_FLASH 
#ifdef RALINK_UPGRADE_BY_SERIAL
			|| type == SEL_LOAD_BOOT_WRITE_FLASH_BY_SERIAL
#endif
			) {
		if(type == SEL_LOAD_BOOT_SDRAM)
			argv[1] = "0x80200000";
		else
			argv[1] = "0x80100000";
		printf("\tInput Uboot filename ");
		//argv[2] = "uboot.bin";
		strncpy(argv[2], "uboot.bin", ARGV_LEN);
	}
	else if (type == SEL_LOAD_LINUX_WRITE_FLASH) {
		argv[1] = "0x80100000";
		printf("\tInput Linux Kernel filename ");
		//argv[2] = "uImage"; winfred: use strncpy instead to prevent the buffer overflow at copy_filename later
		strncpy(argv[2], "uImage", ARGV_LEN);
	}
	else if (type == SEL_LOAD_LINUX_SDRAM ) {
		/* bruce to support ramdisk */
		argv[1] = MK_STR(TFTP_UPLOAD_RAM_ADDRESS);
		printf("\tInput Linux Kernel filename ");
		//argv[2] = "uImage";
		strncpy(argv[2], "uImage", ARGV_LEN);
	}

	s = getenv("bootfile");
	if (s != NULL) {
		memcpy(file, s, strlen(s));
		memcpy(default_file, s, strlen(s));
	}
	printf("(%s) ", file);
	input_value(file);
	if (file[0] == '\0')
		return 1;
	copy_filename (argv[2], file, sizeof(file));
	setenv("bootfile", file);
	if (strcmp(default_file, file) != 0)
		modifies++;

	return 0;
}
#endif /* TFTP_SUPPORT */

static inline void trigger_hw_reset(void)
{
}

static int flash_uboot_image(ulong image_ptr, ulong image_size)
{
	int rrc = 0;

#if defined (CFG_ENV_IS_IN_SPI)
	printf("\n Copy %d bytes to Flash... \n", image_size);
	rrc = raspi_erase_write((char *)image_ptr, 0, image_size);
#else
#error Unsupported Platform
#endif

	if (rrc) {
#if defined (CFG_ENV_IS_IN_SPI)
		printf("Error code: %d\n", rrc);
#endif
		return -1;
	}

	printf("%d bytes flashed\n", image_size);

	return 0;
}

int flash_kernel_image(ulong image_ptr, ulong image_size)
{
	int rrc = 0;
	ulong e_end;

	if (image_size < LINUX_FILE_SIZE_MIN) {
		printf("%s image size %d too small!\n", "Linux", image_size);
		return -1;
	}

	if (verify_kernel_image(image_ptr, NULL, NULL, NULL, 1) <= 0) {
		printf("Check image error!\n");
		return -1;
	}

#if defined (CFG_ENV_IS_IN_SPI)
	printf("\n Copy %d bytes to Flash... \n", image_size);
	rrc = raspi_erase_write((char *)image_ptr, CFG_KERNEL_OFFSET, image_size);
#else
#error Unsupported Platform
#endif

	if (rrc) {
#if defined (CFG_ENV_IS_IN_SPI)
		printf("Error code: %d\n", rrc);
#endif
		return -1;
	}

	printf("%d bytes flashed\n", image_size);

	return 0;
}

#if defined (RALINK_USB) || defined (MTK_USB)
int flash_kernel_image_from_usb(cmd_tbl_t *cmdtp)
{
	char *argv[5];
	char addr_str[16], dev_str[4];
	int i, done;

	argv[1] = "start";

	do_usb(cmdtp, 0, 2, argv);
	if (usb_stor_curr_dev < 0) {
		printf("\n No USB Storage found. Upgrade FW failed!\n");
		return -1;
	}

	sprintf(addr_str, "0x%X", CFG_LOAD_ADDR);

	argv[1] = "usb";
	argv[2] = &dev_str[0];
	argv[3] = &addr_str[0];
	argv[4] = "root_uImage";

	done = 0;
	for (i = 0; i < USB_MAX_STOR_DEV; ++i) {
		block_dev_desc_t *stor_dev = usb_stor_get_dev(i);
		if (stor_dev->type != DEV_TYPE_UNKNOWN) {
			sprintf(dev_str, "%d", i);
			if (do_fat_fsload(cmdtp, 0, 5, argv) == 0) {
				done = 1;
				break;
			}
		}
	}

	if (!done) {
		printf("\n Upgrade FW from USB storage failed!\n");
		return -1;
	}

	NetBootFileXferSize = simple_strtoul(getenv("filesize"), NULL, 16);
	if (flash_kernel_image(CFG_LOAD_ADDR, NetBootFileXferSize) != 0)
		return 1;

	return 0;
}
#endif

void perform_system_reset(void)
{
	printf("\nSYSTEM RESET!!!\n\n");
	udelay(500);
	do_reset(NULL, 0, 0, NULL);
}

typedef int(*gpio_button_f)(void);
typedef void(*gpio_led_f)(void);

#define INT_DIV 32

static int count_button_press(int interval, const gpio_button_f button_func, int blink_interval, gpio_led_f led_on, gpio_led_f led_off)
{
	const int jitter = INT_DIV *2;
	int state_time[2];
	int button_state;
	int n, led_state, remainder;

	state_time[0] = 0;
	state_time[1] = 0;

	remainder = interval % INT_DIV;
	interval = interval / INT_DIV;

	if (blink_interval) {
		/* sanity checks */
		if (led_on == NULL || led_off == NULL) {
			blink_interval = 0;
		}
	}

	button_state = button_func();

	if (led_on && button_state != 0) {
		led_state = 0;
		led_on();
	} else {
		led_state = 1;
	}

	for ( n = 0; n < interval; n++ ) {
		state_time[button_state] += INT_DIV;
		if (button_state != 0) {
			/* try to ignore short releases */
			if (state_time[0] <= jitter) {
				state_time[1] += state_time[0];
				state_time[0] = 0;
			}
		} else {
			if (state_time[0] > jitter) {
				break; /* button released long enough */
			}
			if (state_time[1] == 0) {
				break; /* never pressed */
			}
		}
		if (blink_interval) {
			int new_led_state = (state_time[1] / blink_interval) & 1;
			if (led_state != new_led_state) {
				led_state = new_led_state;
				if (new_led_state) {
					led_off();
				} else {
					led_on();
				}
			}
		}
		udelay( INT_DIV * 1000 );
		button_state = button_func();
	}

	if (state_time[1] < jitter) {
		state_time[1] = 0;
	} else {
		state_time[1] += remainder;
	}

	if (led_off && led_state == 0) {
		led_off();
	}

	return state_time[1];
}

/************************************************************************
 *
 * This is the next part if the initialization sequence: we are now
 * running from RAM and have a "normal" C environment, i. e. global
 * data can be written, BSS has been cleared, the stack size in not
 * that critical any more, etc.
 *
 ************************************************************************
 */

gd_t gd_data;

static void launch_recovery(void)
{
	eth_initialize(gd->bd);
#if defined(HTTPD_SUPPORT)
	NetLoopHttpd();
#elif defined(TFTP_SUPPORT)
	NetLoop(TFTPD);
#else
#error "no recovery option available"
#endif
	perform_system_reset();
}

void __attribute__((nomips16)) board_init_r(gd_t *id, ulong dest_addr)
{
	cmd_tbl_t *cmdtp;
	ulong size;
	extern void malloc_bin_reloc (void);
#ifndef CFG_ENV_IS_NOWHERE
	extern char * env_name_spec;
#endif
	char *s, *e;
	bd_t *bd;
	int i;
	int timer1;
	unsigned char confirm=0;
	int is_soft_reset=0;
	char addr_str[16];
	ulong load_address;

	int wps_button_time;
	int rst_button_time;

	char *uboot_file = "uboot.bin";

	u32 reg;
	u32 config1,lsize,icache_linesz,icache_sets,icache_ways,icache_size;
	u32 dcache_linesz,dcache_sets,dcache_ways,dcache_size;

	memcpy((void *)(CFG_SDRAM_BASE + DRAM_SIZE*0x100000 - 0x10000), (void *)gd, sizeof(*gd));
	gd = (gd_t *)(CFG_SDRAM_BASE + DRAM_SIZE*0x100000- 0x10000);//&gd_data;

	gd->flags |= GD_FLG_RELOC;	/* tell others: relocation done */

	Init_System_Mode(); /*  Get CPU rate */

#if defined(CONFIG_USB_XHCI)
	void config_usb_mtk_xhci(void);
	config_usb_mtk_xhci();
#endif

	reg = RALINK_REG(RT2880_RSTSTAT_REG);
	if(reg & RT2880_WDRST ){
		printf("***********************\n");
		printf("Watchdog Reset Occurred\n");
		printf("***********************\n");
		RALINK_REG(RT2880_RSTSTAT_REG)|=RT2880_WDRST;
		RALINK_REG(RT2880_RSTSTAT_REG)&=~RT2880_WDRST;
	}else if(reg & RT2880_SWSYSRST){
		printf("******************************\n");
		printf("Software System Reset Occurred\n");
		printf("******************************\n");
		RALINK_REG(RT2880_RSTSTAT_REG)|=RT2880_SWSYSRST;
		RALINK_REG(RT2880_RSTSTAT_REG)&=~RT2880_SWSYSRST;
	}else if (reg & RT2880_SWCPURST){
		printf("***************************\n");
		printf("Software CPU Reset Occurred\n");
		printf("***************************\n");
		RALINK_REG(RT2880_RSTSTAT_REG)|=RT2880_SWCPURST;
		RALINK_REG(RT2880_RSTSTAT_REG)&=~RT2880_SWCPURST;
	}

	if(reg & (RT2880_WDRST|RT2880_SWSYSRST|RT2880_SWCPURST)){
		is_soft_reset=1;
		trigger_hw_reset();
	}

//#ifdef DEBUG
	debug ("Now running in RAM - U-Boot at: %08x\n", dest_addr);
//#endif
	gd->reloc_off = dest_addr - CFG_MONITOR_BASE;

	monitor_flash_len = (ulong)&uboot_end_data - dest_addr;
//#ifdef DEBUG
	debug("\n monitor_flash_len =%d \n",monitor_flash_len);
//#endif
	/*
	 * We have to relocate the command table manually
	 */
	for (cmdtp = &__u_boot_cmd_start; cmdtp !=  &__u_boot_cmd_end; cmdtp++) {
		ulong addr;

		addr = (ulong) (cmdtp->cmd) + gd->reloc_off;
#ifdef DEBUG
		printf ("Command \"%s\": 0x%08lx => 0x%08lx\n",
				cmdtp->name, (ulong) (cmdtp->cmd), addr);
#endif
		cmdtp->cmd =
			(int (*)(struct cmd_tbl_s *, int, int, char *[]))addr;

		addr = (ulong)(cmdtp->name) + gd->reloc_off;
		cmdtp->name = (char *)addr;

		if (cmdtp->usage) {
			addr = (ulong)(cmdtp->usage) + gd->reloc_off;
			cmdtp->usage = (char *)addr;
		}
#ifdef	CFG_LONGHELP
		if (cmdtp->help) {
			addr = (ulong)(cmdtp->help) + gd->reloc_off;
			cmdtp->help = (char *)addr;
		}
#endif

	}
	/* there are some other pointer constants we must deal with */
#ifndef CFG_ENV_IS_NOWHERE
	env_name_spec += gd->reloc_off;
#endif

	bd = gd->bd;
#if defined (CFG_ENV_IS_IN_SPI)
	if ((size = raspi_init()) == (ulong)-1) {
		printf("raspi_init fail\n");
		while(1);
	}
	bd->bi_flashstart = CFG_FLASH_BASE;
	bd->bi_flashsize = size;
	bd->bi_flashoffset = 0;
#else //CFG_ENV_IS_IN_FLASH
	/* configure available FLASH banks */
	size = flash_init();

	bd->bi_flashstart = CFG_FLASH_BASE;
	bd->bi_flashsize = size;
#if CFG_MONITOR_BASE == CFG_FLASH_BASE
	bd->bi_flashoffset = monitor_flash_len;	/* reserved area for U-Boot */
#else
	bd->bi_flashoffset = 0;
#endif
#endif //CFG_ENV_IS_IN_FLASH

	/* initialize malloc() area */
	debug("initialize malloc() area...");
#ifdef CONFIG_QMALLOC
	m_init((unsigned char*)CFG_MONITOR_BASE + gd->reloc_off - TOTAL_MALLOC_LEN, TOTAL_MALLOC_LEN);
#else
	mem_malloc_init();
	malloc_bin_reloc();
#endif
	debug("done\n");

#if defined (CFG_ENV_IS_IN_SPI)
	spi_env_init();
#else
	flash_env_init();
#endif //CFG_ENV_IS_IN_FLASH

	/* relocate environment function pointers etc. */
	debug("env_relocate()...\n");
	env_relocate();
	debug("env_relocate() done\n");

	/* board MAC address */
	s = getenv ("ethaddr");
	for (i = 0; i < 6; ++i) {
		bd->bi_enetaddr[i] = s ? simple_strtoul (s, &e, 16) : 0;
		if (s)
			s = (*e) ? e + 1 : e;
	}

	/* IP Address */
	bd->bi_ip_addr = getenv_IPaddr("ipaddr");

	/** leave this here (after malloc(), environment and PCI are working) **/
	/* Initialize devices */
	devices_init ();

	jumptable_init ();

	/* Initialize the console (after the relocation and devices init) */
	console_init_r ();

	/* Initialize from environment */
	if ((s = getenv ("loadaddr")) != NULL) {
		load_addr = simple_strtoul (s, NULL, 16);
	}
#if (CONFIG_COMMANDS & CFG_CMD_NET)
	if ((s = getenv ("bootfile")) != NULL) {
		copy_filename (BootFile, s, sizeof (BootFile));
	}
#endif	/* CFG_CMD_NET */

	{
	printf("============================================ \n");
	printf("%s U-Boot Version: %s\n", RLT_MTK_VENDOR_NAME, RALINK_LOCAL_VERSION);
	printf("-------------------------------------------- \n");
#ifdef RALINK_DUAL_CORE_FUN	
	printf("%s %s %s %s\n", CHIP_TYPE, RALINK_REG(RT2880_CHIP_REV_ID_REG)>>16&0x1 ? "MT7621A" : "MT7621N", "DualCore", GMAC_MODE);
#else
	if(RALINK_REG(RT2880_CHIP_REV_ID_REG)>>17&0x1) {
		printf("%s %s %s %s\n", CHIP_TYPE, RALINK_REG(RT2880_CHIP_REV_ID_REG)>>16&0x1 ? "MT7621A" : "MT7621N", "SingleCore", GMAC_MODE);
	}else {
		printf("%s %s%s %s\n", CHIP_TYPE, RALINK_REG(RT2880_CHIP_REV_ID_REG)>>16&0x1 ? "MT7621A" : "MT7621N", "S", GMAC_MODE);
	}
#endif
	printf("DRAM_CONF_FROM: %s \n", RALINK_REG(RT2880_SYSCFG_REG)>>9&0x1 ? "Auto-Detection" : "EEPROM");
	printf("DRAM_TYPE: %s \n", RALINK_REG(RT2880_SYSCFG_REG)>>4&0x1 ? "DDR2": "DDR3");
	printf("DRAM bus: %d bit\n", DRAM_BUS);
	printf("Xtal Mode=%d OCP Ratio=%s\n", RALINK_REG(RT2880_SYSCFG_REG)>>6&0x7, RALINK_REG(RT2880_SYSCFG_REG)>>5&0x1 ? "1/4":"1/3");
	printf("%s\n", FLASH_MSG);
	printf("%s\n", "Date:" __DATE__ "  Time:" __TIME__ );
	printf("============================================ \n");
	}

	config1 = read_32bit_cp0_register_with_select1(CP0_CONFIG);

	if ((lsize = ((config1 >> 19) & 7)))
		icache_linesz = 2 << lsize;
	else
		icache_linesz = lsize;
	icache_sets = 64 << ((config1 >> 22) & 7);
	icache_ways = 1 + ((config1 >> 16) & 7);

	icache_size = icache_sets *
		icache_ways *
		icache_linesz;

	printf("icache: sets:%d, ways:%d, linesz:%d, total:%d\n", 
			icache_sets, icache_ways, icache_linesz, icache_size);

	/*
	 * Now probe the MIPS32 / MIPS64 data cache.
	 */

	if ((lsize = ((config1 >> 10) & 7)))
		dcache_linesz = 2 << lsize;
	else
		dcache_linesz = lsize;
	dcache_sets = 64 << ((config1 >> 13) & 7);
	dcache_ways = 1 + ((config1 >> 7) & 7);

	dcache_size = dcache_sets *
		dcache_ways *
		dcache_linesz;

	printf("dcache: sets:%d, ways:%d, linesz:%d, total:%d \n", 
			dcache_sets, dcache_ways, dcache_linesz, dcache_size);

	debug("\n #### The CPU freq = %d MHZ #### \n", mips_cpu_feq/1000/1000);

#if defined (ON_BOARD_4096M_DRAM_COMPONENT) 
	debug(" estimate memory size = %d Mbytes\n", gd->ram_size/1024/1024 + 64);
#else
	debug(" estimate memory size = %d Mbytes\n", gd->ram_size/1024/1024 );
#endif

	LED_HIDE_ALL();

	setup_internal_gsw();

	LANWANPartition();

	/*config bootdelay via environment parameter: bootdelay */
	{
	    const char *s = getenv ("bootdelay");
	    timer1 = s ? (int)simple_strtol(s, NULL, 10) : CONFIG_BOOTDELAY;
	}

	if (timer1 < 0) {
		timer1 = 0;
	} else if (timer1 > 2 * 60) {
		timer1 = 2 * 60; /* limit timeout */
	}

	OperationSelectBanner();

	wps_button_time = 0;
	rst_button_time = 0;

	BootType = '3'; /* TODO: change to definitions */

	timer1 *= 100; /* cycle every 10ms */
	for ( i = 0; i < timer1; i++ ) {
		if ( tstc() != 0 ) { /* we got a key press	*/
			BootType = getc();
			printf( "key readed = %d\n", BootType );
			if ( (BootType < '0' || BootType > '5') && (BootType != '7') && (BootType != '8') && (BootType != '9') )
				BootType = '3';
			printf( "\n\rYou choosed %c\n\n", BootType );
			break;
		}
		udelay(10 * 1000);
		/* detect buttons */
		if (DETECT_BTN_WPS()) {
			wps_button_time += count_button_press(5000, DETECT_BTN_WPS, 200, LED_WPS_ON, LED_WPS_OFF);
			if (wps_button_time == 5000) {
				int status = rootfs_func(1); /* check status */
				if (status == 0) {
					/* if it can be reset - perform button press detection */
					rst_button_time += count_button_press(3000, DETECT_BTN_WPS, 125, LED_ALERT_ON, LED_ALERT_OFF);
				} else {
					/* already reset or not supported */
				}
				break;
			}
		}
	}

	putc ('\n');

	eth_preinit(); /* so eth_halt() inside "bootm" will not crash */

	if (wps_button_time) {
		if (wps_button_time < 5000) {
			setenv("autostart", "no");
			LED_WPS_ON();
#if defined (RALINK_USB) || defined (MTK_USB)
			if (flash_kernel_image_from_usb(cmdtp) == 0) {
				perform_system_reset();
				return; /* to nowhere */
			}
#endif
			/* Wait forever for an image */
			launch_recovery();
		} else if (wps_button_time < 8000) {
			LED_ALERT_ON();
			rootfs_func(0); /* erase rootfs */
			udelay(500 * 1000);
			LED_ALERT_OFF();
			udelay(500 * 1000);
			perform_system_reset();
		}
	}

	if (BootType == '3') {
		char *argv[2];

		printf("   \n%d: System Boot system code via Flash.\n", 3);
		sprintf(addr_str, "0x%X", CFG_KERN_ADDR);
		argv[0] = "";
		argv[1] = &addr_str[0];
		
		if (do_bootm(cmdtp, 0, 2, argv) > 0) {
			for (i = 0; i < 3; i++) {
				LED_ALERT_ON();
				udelay(1000 * 1000);
				LED_ALERT_OFF();
			}
			launch_recovery();
		}
	} else {
		char *argv[4];
		int argc = 3;

		argv[2] = &file_name_space[0];
		memset(file_name_space,0,ARGV_LEN);

		eth_initialize(gd->bd);

		switch (BootType) {
#ifdef TFTP_SUPPORT
		case '1':
			printf("   \n%d: System Load Linux to SDRAM via TFTP. \n", SEL_LOAD_LINUX_SDRAM);
			tftp_config(SEL_LOAD_LINUX_SDRAM, argv);

			setenv("autostart", "yes");

			argc = 3;
			do_tftpb(cmdtp, 0, argc, argv);
			break;

		case '2':
retry_kernel_tftp:
			printf("   \n%d: System Load %s then write to Flash via %s. \n", SEL_LOAD_LINUX_WRITE_FLASH, "Linux", "TFTP");
			printf(" Warning!! Erase %s in Flash then burn new one. Are you sure? (Y/N)\n", "Linux");
			confirm = getc();
			if (confirm != 'y' && confirm != 'Y') {
				printf(" Operation terminated\n");
				break;
			}
			tftp_config(SEL_LOAD_LINUX_WRITE_FLASH, argv);

			setenv("autostart", "no");

			argc = 3;
			do_tftpb(cmdtp, 0, argc, argv);

			load_address = simple_strtoul(argv[1], NULL, 16);
			if (flash_kernel_image(load_address, NetBootFileXferSize) != 0)
				goto retry_kernel_tftp;

			//bootm BC040000
			argc = 2;
			sprintf(addr_str, "0x%X", CFG_KERN_ADDR);
			argv[1] = &addr_str[0];
			do_bootm(cmdtp, 0, argc, argv);
			break;
#endif /* TFTP_SUPPORT */

#ifdef RALINK_CMDLINE
		case '4':
			printf("   \n%d: System Enter Boot Command Line Interface.\n", SEL_ENTER_CLI);
			printf ("\n%s\n", version_string);
			load_addr = CFG_KERN_ADDR;
			/* main_loop() can return to retry autoboot, if so just run it again. */
			for (;;) {
				main_loop ();
			}
			break;
#endif /* RALINK_CMDLINE */

#ifdef RALINK_UPGRADE_BY_SERIAL
		case '7':
retry_uboot_serial:
			printf("   \n%d: System Load %s then write to Flash via %s. \n", SEL_LOAD_BOOT_WRITE_FLASH_BY_SERIAL, "Boot Loader", "Serial");
			printf(" Warning!! Erase %s in Flash then burn new one. Are you sure? (Y/N)\n", "Boot Loader");
			confirm = getc();
			if (confirm != 'y' && confirm != 'Y') {
				printf(" Operation terminated\n");
				break;
			}

			setenv("autostart", "no");

			argc = 1;
			if (do_load_serial_bin(cmdtp, 0, argc, argv) == 1) {
				printf("\n Download aborted!\n");
				break;
			}

			NetBootFileXferSize = simple_strtoul(getenv("filesize"), NULL, 16);
			if (verify_uboot_image((u32 *)CFG_LOAD_ADDR, NetBootFileXferSize) != 0)
				goto retry_uboot_serial;

			if (flash_uboot_image(CFG_LOAD_ADDR, NetBootFileXferSize) != 0)
				goto retry_uboot_serial;

			break;

		case '0':
retry_kernel_serial:
			printf("   \n%d: System Load %s then write to Flash via %s. \n", SEL_LOAD_LINUX_WRITE_FLASH_BY_SERIAL, "Linux", "Serial");
			printf(" Warning!! Erase %s in Flash then burn new one. Are you sure? (Y/N)\n", "Linux");
			confirm = getc();
			if (confirm != 'y' && confirm != 'Y') {
				printf(" Operation terminated\n");
				break;
			}

			setenv("autostart", "no");

			argc = 1;
			if (do_load_serial_bin(cmdtp, 0, argc, argv) == 1) {
				printf("\n Download aborted!\n");
				break;
			}

			NetBootFileXferSize = simple_strtoul(getenv("filesize"), NULL, 16);
			if (flash_kernel_image(CFG_LOAD_ADDR, NetBootFileXferSize) != 0)
				goto retry_kernel_serial;

			break;
#endif /* RALINK_UPGRADE_BY_SERIAL */

#ifdef TFTP_SUPPORT
#if 0
		case '8':
			printf("   \n%d: System Load UBoot to SDRAM via TFTP. \n", SEL_LOAD_BOOT_SDRAM);
			tftp_config(SEL_LOAD_BOOT_SDRAM, argv);
			setenv("autostart", "yes");
			argc = 3;
			do_tftpb(cmdtp, 0, argc, argv);
			break;
#endif
		case '9':
retry_uboot_tftp:
			printf("   \n%d: System Load %s then write to Flash via %s. \n", SEL_LOAD_BOOT_WRITE_FLASH, "Boot Loader", "TFTP");
			printf(" Warning!! Erase %s in Flash then burn new one. Are you sure? (Y/N)\n", "Boot Loader");
			confirm = getc();
			if (confirm != 'y' && confirm != 'Y') {
				printf(" Operation terminated\n");
				break;
			}
			setenv("bootfile", uboot_file);
			tftp_config(SEL_LOAD_BOOT_WRITE_FLASH, argv);
			setenv("autostart", "no");

			argc = 3;
			do_tftpb(cmdtp, 0, argc, argv);

			load_address = simple_strtoul(argv[1], NULL, 16);
			if (verify_uboot_image((u32 *)load_address, NetBootFileXferSize) != 0)
				goto retry_uboot_tftp;

			if (flash_uboot_image(load_address, NetBootFileXferSize) != 0)
				goto retry_uboot_tftp;

			break;
#endif /* TFTP_SUPPORT */

#if defined (RALINK_USB) || defined (MTK_USB)
		case '5':
			printf("   \n%d: System Load %s then write to Flash via %s. \n", SEL_LOAD_LINUX_WRITE_FLASH_BY_USB, "Linux", "USB Storage");
			printf(" Warning!! Erase %s in Flash then burn new one. Are you sure? (Y/N)\n", "Linux");
			confirm = getc();
			if (confirm != 'y' && confirm != 'Y') {
				printf(" Operation terminated\n");
				break;
			}

			setenv("autostart", "no");

			flash_kernel_image_from_usb(cmdtp);

			break;
#endif // RALINK_UPGRADE_BY_USB //

		default:
			printf("   \nSystem Boot Linux via Flash.\n");
			do_bootm(cmdtp, 0, 1, argv);
			break;            
		} /* end of switch */   

		perform_system_reset();

	} /* end of else */

	/* NOTREACHED - no way out of command loop except booting */
}


void hang (void)
{
	puts ("### ERROR ### Please RESET the board ###\n");
	for (;;);
}

#if defined (RALINK_RW_RF_REG_FUN)
#if defined (MT7620_ASIC_BOARD)
#define RF_CSR_CFG      0xb0180500
#define RF_CSR_KICK     (1<<0)
int rw_rf_reg(int write, int reg, int *data)
{
	u32	rfcsr, i = 0;

	while (1) {
		rfcsr = RALINK_REG(RF_CSR_CFG);
		if (! (rfcsr & (u32)RF_CSR_KICK) )
			break;
		if (++i > 10000) {
			puts("Warning: Abort rw rf register: too busy\n");
			return -1;
		}
	}
	rfcsr = (u32)(RF_CSR_KICK | ((reg & 0x3f) << 16)  | ((*data & 0xff) << 8));
	if (write)
		rfcsr |= 0x10;

	RALINK_REG(RF_CSR_CFG) = cpu_to_le32(rfcsr);
	i = 0;
	while (1) {
		rfcsr = RALINK_REG(RF_CSR_CFG);
		if (! (rfcsr & (u32)RF_CSR_KICK) )
			break;
		if (++i > 10000) {
			puts("Warning: still busy\n");
			return -1;
		}
	}

	rfcsr = RALINK_REG(RF_CSR_CFG);
	if (((rfcsr & 0x3f0000) >> 16) != (reg & 0x3f)) {
		puts("Error: rw register failed\n");
		return -1;
	}
	*data = (int)( (rfcsr & 0xff00) >> 8) ;
	return 0;
}
#else
#define RF_CSR_CFG      0xb0180500
#define RF_CSR_KICK     (1<<17)
int rw_rf_reg(int write, int reg, int *data)
{
	u32	rfcsr, i = 0;

	while (1) {
		rfcsr = RALINK_REG(RF_CSR_CFG);
		if (! (rfcsr & (u32)RF_CSR_KICK) )
			break;
		if (++i > 10000) {
			puts("Warning: Abort rw rf register: too busy\n");
			return -1;
		}
	}


	rfcsr = (u32)(RF_CSR_KICK | ((reg & 0x3f) << 8)  | (*data & 0xff));
	if (write)
		rfcsr |= 0x10000;

	RALINK_REG(RF_CSR_CFG) = cpu_to_le32(rfcsr);

	i = 0;
	while (1) {
		rfcsr = RALINK_REG(RF_CSR_CFG);
		if (! (rfcsr & (u32)RF_CSR_KICK) )
			break;
		if (++i > 10000) {
			puts("Warning: still busy\n");
			return -1;
		}
	}

	rfcsr = RALINK_REG(RF_CSR_CFG);

	if (((rfcsr&0x1f00) >> 8) != (reg & 0x1f)) {
		puts("Error: rw register failed\n");
		return -1;
	}
	*data = (int)(rfcsr & 0xff);

	return 0;
}
#endif
#endif

#ifdef RALINK_RW_RF_REG_FUN
#ifdef RALINK_CMDLINE
int do_rw_rf(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	int write, reg, data;

	if ((argv[1][0] == 'r' || argv[1][0] == 'R') && (argc == 3)) {
		write = 0;
		reg = (int)simple_strtoul(argv[2], NULL, 10);
		data = 0;
	}
	else if ((argv[1][0] == 'w' || argv[1][0] == 'W') && (argc == 4)) {
		write = 1;
		reg = (int)simple_strtoul(argv[2], NULL, 10);
		data = (int)simple_strtoul(argv[3], NULL, 16);
	}
	else {
		printf ("Usage:\n%s\n", cmdtp->usage);
		return 1;
	}

	rw_rf_reg(write, reg, &data);
	if (!write)
		printf("rf reg <%d> = 0x%x\n", reg, data);
	return 0;
}

U_BOOT_CMD(
	rf,     4,     1,      do_rw_rf,
	"rf      - read/write rf register\n",
	"usage:\n"
	"rf r <reg>        - read rf register\n"
	"rf w <reg> <data> - write rf register (reg: decimal, data: hex)\n"
);
#endif // RALINK_CMDLINE //
#endif

#if defined(CONFIG_USB_XHCI)
void config_usb_mtk_xhci(void)
{
	u32	regValue;

	regValue = RALINK_REG(RALINK_SYSCTL_BASE + 0x10);
	regValue = (regValue >> 6) & 0x7;
	if(regValue >= 6) { //25Mhz Xtal
		printf("\nConfig XHCI 25M PLL \n");
		RALINK_REG(0xbe1d0784) = 0x20201a;
		RALINK_REG(0xbe1d0c20) = 0x80004;
		RALINK_REG(0xbe1d0c1c) = 0x18181819;
		RALINK_REG(0xbe1d0c24) = 0x18000000;
		RALINK_REG(0xbe1d0c38) = 0x25004a;
		RALINK_REG(0xbe1d0c40) = 0x48004a;
		RALINK_REG(0xbe1d0b24) = 0x190;
		RALINK_REG(0xbe1d0b10) = 0x1c000000;
		RALINK_REG(0xbe1d0b04) = 0x20000004;
		RALINK_REG(0xbe1d0b08) = 0xf203200;

		RALINK_REG(0xbe1d0b2c) = 0x1400028;
		//RALINK_REG(0xbe1d0a30) =;
		RALINK_REG(0xbe1d0a40) = 0xffff0001;
		RALINK_REG(0xbe1d0a44) = 0x60001;
	} else if (regValue >=3 ) { // 40 Mhz
		printf("\nConfig XHCI 40M PLL \n");
		RALINK_REG(0xbe1d0784) = 0x20201a;
		RALINK_REG(0xbe1d0c20) = 0x80104;
		RALINK_REG(0xbe1d0c1c) = 0x1818181e;
		RALINK_REG(0xbe1d0c24) = 0x1e400000;
		RALINK_REG(0xbe1d0c38) = 0x250073;
		RALINK_REG(0xbe1d0c40) = 0x71004a;
		RALINK_REG(0xbe1d0b24) = 0x140;
		RALINK_REG(0xbe1d0b10) = 0x23800000;
		RALINK_REG(0xbe1d0b04) = 0x20000005;
		RALINK_REG(0xbe1d0b08) = 0x12203200;
	
		RALINK_REG(0xbe1d0b2c) = 0x1400028;
		//RALINK_REG(0xbe1d0a30) =;
		RALINK_REG(0xbe1d0a40) = 0xffff0001;
		RALINK_REG(0xbe1d0a44) = 0x60001;
	} else { //20Mhz Xtal
		/* TODO */
	}
}
#endif /* CONFIG_USB_XHC */

#if defined (CONFIG_DDR_CAL)
#include <asm-mips/mipsregs.h>
#include <asm-mips/cacheops.h>
#include <asm/mipsregs.h>
#include <asm/cache.h>

#if defined (CONFIG_TINY_UBOOT)
__attribute__((nomips16))
#endif
static inline void cal_memcpy(void* src, void* dst, unsigned int size)
{
	int i;
	unsigned char* psrc = (unsigned char*)src, *pdst=(unsigned char*)dst;
	for (i = 0; i < size; i++, psrc++, pdst++)
		(*pdst) = (*psrc);
	return;
}
#if defined (CONFIG_TINY_UBOOT)
__attribute__((nomips16))
#endif
static inline void cal_memset(void* src, unsigned char pat, unsigned int size)
{
	int i;
	unsigned char* psrc = (unsigned char*)src;
	for (i = 0; i < size; i++, psrc++)
		(*psrc) = pat;
	return;
}

#define pref_op(hint,addr)						\
	 __asm__ __volatile__(						\
	"       .set    push                                    \n"	\
	"       .set    noreorder                               \n"	\
	"       pref   %0, %1                                  \n"	\
	"       .set    pop                                     \n"	\
	:								\
	: "i" (hint), "R" (*(unsigned char *)(addr)))	
		
#define cache_op(op,addr)						\
	 __asm__ __volatile__(						\
	"       .set    push                                    \n"	\
	"       .set    noreorder                               \n"	\
	"       .set    mips3\n\t                               \n"	\
	"       cache   %0, %1                                  \n"	\
	"       .set    pop                                     \n"	\
	:								\
	: "i" (op), "R" (*(unsigned char *)(addr)))	
	
__attribute__((nomips16)) static void inline cal_invalidate_dcache_range(ulong start_addr, ulong stop)
{
	unsigned long lsize = CONFIG_SYS_CACHELINE_SIZE;
	unsigned long addr = start_addr & ~(lsize - 1);
	unsigned long aend = (stop - 1) & ~(lsize - 1);

	while (1) {
		cache_op(HIT_INVALIDATE_D, addr);
		if (addr == aend)
			break;
		addr += lsize;
	}
}	

#if defined (CONFIG_TINY_UBOOT)
__attribute__((nomips16))
#endif
static void inline cal_patgen(unsigned long* start_addr, unsigned int size, unsigned bias)
{
	int i = 0;
	for (i = 0; i < size; i++)
		start_addr[i] = ((ulong)start_addr+i+bias);
		
	return;	
}	

#define NUM_OF_CACHELINE	128
#define MIN_START	6
#define MIN_FINE_START	0xF
#define MAX_START 7
#define MAX_FINE_START	0x0
#define cal_debug debug
								
#define HWDLL_FIXED	1
#if defined (HWDLL_FIXED)								
#define DU_COARSE_WIDTH	16
#define DU_FINE_WIDTH 16
#define C2F_RATIO 8
#define HWDLL_AVG	1
#define HWDLL_LV	1
//#define HWDLL_HV	1
#define HWDLL_MINSCAN	1
#endif

#define MAX_TEST_LOOP   8

__attribute__((nomips16)) void dram_cali(void)
{
#if defined(ON_BOARD_64M_DRAM_COMPONENT)
	#define DRAM_BUTTOM 0x800000
#endif
#if defined(ON_BOARD_128M_DRAM_COMPONENT)
	#define DRAM_BUTTOM 0x1000000
#endif
#if defined(ON_BOARD_256M_DRAM_COMPONENT)
	#define DRAM_BUTTOM 0x2000000
#endif
#if defined(ON_BOARD_512M_DRAM_COMPONENT)
	#define DRAM_BUTTOM 0x4000000
#endif
#if defined(ON_BOARD_1024M_DRAM_COMPONENT)
	#define DRAM_BUTTOM 0x8000000
#endif
#if defined(ON_BOARD_1024M_KGD_DRAM_COMPONENT)
	#define DRAM_BUTTOM 0x8000000
#endif
#if defined(ON_BOARD_2048M_DRAM_COMPONENT)
	#define DRAM_BUTTOM 0x10000000
#endif

	unsigned int * nc_addr = 0xA0000000+DRAM_BUTTOM-0x0400;
	unsigned int * c_addr = 0x80000000+DRAM_BUTTOM-0x0400;
	unsigned int min_coarse_dqs[2];
	unsigned int max_coarse_dqs[2];
	unsigned int min_fine_dqs[2];
	unsigned int max_fine_dqs[2];
	unsigned int coarse_dqs[2];
	unsigned int fine_dqs[2];
	unsigned int min_dqs[2];
	unsigned int max_dqs[2];
	unsigned int total_min_comp_dqs[2];
	unsigned int total_max_comp_dqs[2];
	unsigned int avg_min_cg_comp_dqs[2];
	unsigned int avg_max_cg_comp_dqs[2];
	unsigned int avg_min_fg_comp_dqs[2];
	unsigned int avg_max_fg_comp_dqs[2];
	unsigned int min_comp_dqs[2][MAX_TEST_LOOP];
	unsigned int max_comp_dqs[2][MAX_TEST_LOOP];
	unsigned int reg = 0, ddr_cfg2_reg = 0, dqs_dly_reg = 0;
	unsigned int reg_avg = 0, reg_with_dll = 0, hw_dll_reg = 0;
	int ret = 0;
	int flag = 0, min_failed_pos[2], max_failed_pos[2], min_fine_failed_pos[2], max_fine_failed_pos[2];
	int i,j, k;
	int dqs = 0;
	unsigned int min_coarse_dqs_bnd, min_fine_dqs_bnd, coarse_dqs_dll, fine_dqs_dll;
	unsigned int reg_minscan = 0;
	unsigned int avg_cg_dly[2],avg_fg_dly[2];
	unsigned int g_min_coarse_dqs_dly[2], g_min_fine_dqs_dly[2];
#if defined(MT7628_FPGA_BOARD) || defined(MT7628_ASIC_BOARD)		
	unsigned int cid = (RALINK_REG(RALINK_SYSCTL_BASE+0xC)>>16)&0x01;
#endif

#if (NUM_OF_CACHELINE > 40)
#else	
	unsigned int cache_pat[8*40];
#endif	
	u32 value, test_count = 0;;
	u32 fdiv = 0, step = 0, frac = 0;

	value = RALINK_REG(RALINK_DYN_CFG0_REG);
	fdiv = (unsigned long)((value>>8)&0x0F);
	if ((CPU_FRAC_DIV < 1) || (CPU_FRAC_DIV > 10))
		frac = (unsigned long)(value&0x0F);
	else
		frac = CPU_FRAC_DIV;
	i = 0;
	
	while(frac < fdiv) {
		value = RALINK_REG(RALINK_DYN_CFG0_REG);
		fdiv = ((value>>8)&0x0F);
		fdiv--;
		value &= ~(0x0F<<8);
		value |= (fdiv<<8);
		RALINK_REG(RALINK_DYN_CFG0_REG) = value;
		udelay(500);
		i++;
		value = RALINK_REG(RALINK_DYN_CFG0_REG);
		fdiv = ((value>>8)&0x0F);
	}	
#if (NUM_OF_CACHELINE > 40)
#else
	cal_memcpy(cache_pat, dram_patterns, 32*6);
	cal_memcpy(cache_pat+32*6, line_toggle_pattern, 32);
	cal_memcpy(cache_pat+32*6+32, pattern_ken, 32*13);
#endif

#if defined (HWDLL_LV)
#if defined (HWDLL_HV)
	RALINK_REG(RALINK_RGCTRL_BASE+0x108) = 0x01300;
	mdelay(100);
#else
	//RALINK_REG(RALINK_RGCTRL_BASE+0x108) = 0x0F00;//0x0d00;//0x0b00;
#endif
	//cal_debug("\nSet [0x10001108]=%08X\n",RALINK_REG(RALINK_RGCTRL_BASE+0x108));
	//mdelay(100);
#endif
	
#if defined(MT7628_FPGA_BOARD) || defined(MT7628_ASIC_BOARD)	
	RALINK_REG(RALINK_MEMCTRL_BASE+0x10) &= ~(0x1<<4);
#else
	RALINK_REG(RALINK_MEMCTRL_BASE+0x18) = &= ~(0x1<<4);
#endif
	ddr_cfg2_reg = RALINK_REG(RALINK_MEMCTRL_BASE+0x48);
	dqs_dly_reg = RALINK_REG(RALINK_MEMCTRL_BASE+0x64);
	RALINK_REG(RALINK_MEMCTRL_BASE+0x48)&=(~((0x3<<28)|(0x3<<26)));

	total_min_comp_dqs[0] = 0;
	total_min_comp_dqs[1] = 0;
	total_max_comp_dqs[0] = 0;
	total_max_comp_dqs[1] = 0;
TEST_LOOP:
	min_coarse_dqs[0] = MIN_START;
	min_coarse_dqs[1] = MIN_START;
	min_fine_dqs[0] = MIN_FINE_START;
	min_fine_dqs[1] = MIN_FINE_START;
	max_coarse_dqs[0] = MAX_START;
	max_coarse_dqs[1] = MAX_START;
	max_fine_dqs[0] = MAX_FINE_START;
	max_fine_dqs[1] = MAX_FINE_START;
	min_failed_pos[0] = 0xFF;
	min_fine_failed_pos[0] = 0;
	min_failed_pos[1] = 0xFF;
	min_fine_failed_pos[1] = 0;
	max_failed_pos[0] = 0xFF;
	max_fine_failed_pos[0] = 0;
	max_failed_pos[1] = 0xFF;
	max_fine_failed_pos[1] = 0;
	dqs = 0;

	// Add by KP, DQS MIN boundary
	reg = RALINK_REG(RALINK_MEMCTRL_BASE+0x20);
	coarse_dqs_dll = (reg & 0xF00) >> 8;
	fine_dqs_dll = (reg & 0xF0) >> 4;
	if (coarse_dqs_dll<=8)
		min_coarse_dqs_bnd = 8 - coarse_dqs_dll;
	else
		min_coarse_dqs_bnd = 0;
	
	if (fine_dqs_dll<=8)
		min_fine_dqs_bnd = 8 - fine_dqs_dll;
	else
		min_fine_dqs_bnd = 0;
	// DQS MIN boundary
 
DQS_CAL:
	flag = 0;
	j = 0;

	for (k = 0; k < 2; k ++)
	{
		unsigned int test_dqs, failed_pos = 0;
		if (k == 0)
			test_dqs = MAX_START;
		else
			test_dqs = MAX_FINE_START;	
		flag = 0;
		do 
		{	
			flag = 0;
			for (nc_addr = 0xA0000000; nc_addr < (0xA0000000+DRAM_BUTTOM-NUM_OF_CACHELINE*32); nc_addr+=((DRAM_BUTTOM>>6)+1*0x400))
			{
				RALINK_REG(RALINK_MEMCTRL_BASE+0x64) = 0x00007474;
				wmb();
				c_addr = (unsigned int*)((ulong)nc_addr & 0xDFFFFFFF);
				cal_memset(((unsigned char*)c_addr), 0x1F, NUM_OF_CACHELINE*32);
#if (NUM_OF_CACHELINE > 40)
				cal_patgen(nc_addr, NUM_OF_CACHELINE*8, 3);
#else			
				cal_memcpy(((unsigned char*)nc_addr), ((unsigned char*)cache_pat), NUM_OF_CACHELINE*32);
#endif			
				
				if (dqs > 0)
					RALINK_REG(RALINK_MEMCTRL_BASE+0x64) = 0x00000074|(((k==1) ? max_coarse_dqs[dqs] : test_dqs)<<12)|(((k==0) ? 0xF : test_dqs)<<8);
				else
					RALINK_REG(RALINK_MEMCTRL_BASE+0x64) = 0x00007400|(((k==1) ? max_coarse_dqs[dqs] : test_dqs)<<4)|(((k==0) ? 0xF : test_dqs)<<0);
				wmb();
				
				cal_invalidate_dcache_range(((unsigned char*)c_addr), ((unsigned char*)c_addr)+NUM_OF_CACHELINE*32);
				wmb();
				for (i = 0; i < NUM_OF_CACHELINE*8; i ++)
				{
					if (i % 8 ==0)
						pref_op(0, &c_addr[i]);
				}		
				for (i = 0; i < NUM_OF_CACHELINE*8; i ++)
				{
#if (NUM_OF_CACHELINE > 40)
					if (c_addr[i] != (ulong)nc_addr+i+3)
#else				
					if (c_addr[i] != cache_pat[i])
#endif				
					{	
						flag = -1;
						failed_pos = i;
						goto MAX_FAILED;
					}
				}
			}
MAX_FAILED:
			if (flag==-1)
			{
				break;
			}
			else
				test_dqs++;
		}while(test_dqs<=0xF);
		
		if (k==0)
		{	
			max_coarse_dqs[dqs] = test_dqs;
			max_failed_pos[dqs] = failed_pos;
		}
		else
		{	
			test_dqs--;
	
			if (test_dqs==MAX_FINE_START-1)
			{
				max_coarse_dqs[dqs]--;
				max_fine_dqs[dqs] = 0xF;	
			}
			else
			{	
				max_fine_dqs[dqs] = test_dqs;
			}
			max_fine_failed_pos[dqs] = failed_pos;
		}	
	}	

	for (k = 0; k < 2; k ++)
	{
		unsigned int test_dqs, failed_pos = 0;
		if (k == 0)
			test_dqs = MIN_START;
		else
			test_dqs = MIN_FINE_START;	
		flag = 0;
		do 
		{
			for (nc_addr = 0xA0000000; nc_addr < (0xA0000000+DRAM_BUTTOM-NUM_OF_CACHELINE*32); (nc_addr+=(DRAM_BUTTOM>>6)+1*0x480))
			{
				RALINK_REG(RALINK_MEMCTRL_BASE+0x64) = 0x00007474;
				wmb();
				c_addr = (unsigned int*)((ulong)nc_addr & 0xDFFFFFFF);
				RALINK_REG(RALINK_MEMCTRL_BASE+0x64) = 0x00007474;
				wmb();
				cal_memset(((unsigned char*)c_addr), 0x1F, NUM_OF_CACHELINE*32);
#if (NUM_OF_CACHELINE > 40)
				cal_patgen(nc_addr, NUM_OF_CACHELINE*8, 1);
#else			
				cal_memcpy(((unsigned char*)nc_addr), ((unsigned char*)cache_pat), NUM_OF_CACHELINE*32);
#endif				
				if (dqs > 0)
					RALINK_REG(RALINK_MEMCTRL_BASE+0x64) = 0x00000074|(((k==1) ? min_coarse_dqs[dqs] : test_dqs)<<12)|(((k==0) ? 0x0 : test_dqs)<<8);
				else
					RALINK_REG(RALINK_MEMCTRL_BASE+0x64) = 0x00007400|(((k==1) ? min_coarse_dqs[dqs] : test_dqs)<<4)|(((k==0) ? 0x0 : test_dqs)<<0);			
				wmb();
				cal_invalidate_dcache_range(((unsigned char*)c_addr), ((unsigned char*)c_addr)+NUM_OF_CACHELINE*32);
				wmb();
				for (i = 0; i < NUM_OF_CACHELINE*8; i ++)
				{
					if (i % 8 ==0)
						pref_op(0, &c_addr[i]);
				}	
				for (i = 0; i < NUM_OF_CACHELINE*8; i ++)
				{
#if (NUM_OF_CACHELINE > 40)
					if (c_addr[i] != (ulong)nc_addr+i+1)	
#else				
					if (c_addr[i] != cache_pat[i])
#endif
					{
						flag = -1;
						failed_pos = i;
						goto MIN_FAILED;
					}
				}
			}
	
MIN_FAILED:
	
			if (k==0)
			{	
				if ((flag==-1)||(test_dqs==min_coarse_dqs_bnd))
				{
					break;
				}
				else
					test_dqs--;
					
				if (test_dqs < min_coarse_dqs_bnd)
					break;	
			}
			else
			{
				if (flag==-1)
				{
					test_dqs++;
					break;
				}
				else if (test_dqs==min_fine_dqs_bnd)
				{
					break;
				}
				else
				{	
					test_dqs--;                    
				}
				
				if (test_dqs < min_fine_dqs_bnd)
					break;
				
			}
		}while(test_dqs>=0);
		
		if (k==0)
		{	
			min_coarse_dqs[dqs] = test_dqs;
			min_failed_pos[dqs] = failed_pos;
		}
		else
		{	
			if (test_dqs==MIN_FINE_START+1)
			{
				min_coarse_dqs[dqs]++;
				min_fine_dqs[dqs] = 0x0;	
			}
			else
			{	
				min_fine_dqs[dqs] = test_dqs;
			}
			min_fine_failed_pos[dqs] = failed_pos;
		}
	}

	min_comp_dqs[dqs][test_count] = (8-min_coarse_dqs[dqs])*8+(8-min_fine_dqs[dqs]);
	total_min_comp_dqs[dqs] += min_comp_dqs[dqs][test_count];
	max_comp_dqs[dqs][test_count] = (max_coarse_dqs[dqs]-8)*8+(max_fine_dqs[dqs]-8);
	total_max_comp_dqs[dqs] += max_comp_dqs[dqs][test_count];

	if (max_comp_dqs[dqs][test_count]+ min_comp_dqs[dqs][test_count] <=(2*8))
	{
		reg_minscan = 0x18180000;
		reg_with_dll = 0x88880000;
		g_min_coarse_dqs_dly[0] = g_min_coarse_dqs_dly[1] = 0;
		g_min_fine_dqs_dly[0] = g_min_fine_dqs_dly[1] = 0;
		hw_dll_reg = RALINK_REG(RALINK_MEMCTRL_BASE+0x20);
		goto FINAL_SETUP;
	}	

	if (dqs==0)
	{
		dqs = 1;	
		goto DQS_CAL;
	}

	for (i=0 ; i < 2; i++)
	{
		unsigned int temp;
		coarse_dqs[i] = (max_coarse_dqs[i] + min_coarse_dqs[i])>>1; 
		temp = (((max_coarse_dqs[i] + min_coarse_dqs[i])%2)*4)  +  ((max_fine_dqs[i] + min_fine_dqs[i])>>1);
		if (temp >= 0x10)
		{
		   coarse_dqs[i] ++;
		   fine_dqs[i] = (temp-0x10) +0x8;
		}
		else
		{
			fine_dqs[i] = temp;
		}
	}
	reg = (coarse_dqs[1]<<12)|(fine_dqs[1]<<8)|(coarse_dqs[0]<<4)|fine_dqs[0];
	
#if defined(MT7628_FPGA_BOARD) || defined(MT7628_ASIC_BOARD)
	if (cid == 1)
		RALINK_REG(RALINK_MEMCTRL_BASE+0x10) &= ~(0x1<<4);
#else
	RALINK_REG(RALINK_MEMCTRL_BASE+0x18) = &= ~(0x1<<4);
#endif
	if (cid == 1) {
		RALINK_REG(RALINK_MEMCTRL_BASE+0x64) = reg;
		RALINK_REG(RALINK_MEMCTRL_BASE+0x48) = ddr_cfg2_reg;
	}
#if defined(MT7628_FPGA_BOARD) || defined(MT7628_ASIC_BOARD)	
	if (cid == 1)
		RALINK_REG(RALINK_MEMCTRL_BASE+0x10) |= (0x1<<4);
#else
	RALINK_REG(RALINK_MEMCTRL_BASE+0x18) |= (0x1<<4);
#endif

	test_count++;


FINAL:
#if defined(MT7628_FPGA_BOARD) || defined(MT7628_ASIC_BOARD)	
	if (cid==1)
#endif	
	{	
		for (j = 0; j < 2; j++)	
			cal_debug("[%02X%02X%02X%02X]",min_coarse_dqs[j],min_fine_dqs[j], max_coarse_dqs[j],max_fine_dqs[j]);
		cal_debug("\nDDR Calibration DQS reg = %08X\n",reg);
		goto EXIT;
	}
	if (test_count < MAX_TEST_LOOP)
		goto TEST_LOOP;

	for (j = 0; j < 2; j++)	
	{
		unsigned int min_count = MAX_TEST_LOOP;
		unsigned int max_count = MAX_TEST_LOOP;
		
		unsigned int tmp_min_comp_dqs = total_min_comp_dqs[j]>>3;
		unsigned int tmp_total_min_comp_dqs = total_min_comp_dqs[j];
		
		unsigned int tmp_max_comp_dqs = total_max_comp_dqs[j]>>3;
		unsigned int tmp_total_max_comp_dqs = total_max_comp_dqs[j];
		
		for (k = 0; k < MAX_TEST_LOOP; k++)
		{
			int diff_min = ((tmp_min_comp_dqs-min_comp_dqs[j][k]) > 0) ? (tmp_min_comp_dqs-min_comp_dqs[j][k]) : (min_comp_dqs[j][k]-tmp_min_comp_dqs);
			int diff_max = ((tmp_max_comp_dqs-max_comp_dqs[j][k]) > 0) ? (tmp_max_comp_dqs-max_comp_dqs[j][k]) : (max_comp_dqs[j][k]-tmp_max_comp_dqs);

			if (diff_min > 5)
			{
				//cal_debug("remove the %d min comp dqs %d (%d)\n" ,k ,min_comp_dqs[j][k],tmp_min_comp_dqs);
				tmp_total_min_comp_dqs-= min_comp_dqs[j][k];
				tmp_total_min_comp_dqs += tmp_min_comp_dqs;
				min_count--;
			}
			if (diff_max > 5)
			{
				//cal_debug("remove the %d (diff=%d) max comp dqs %d (%d)\n" ,k ,diff_max,max_comp_dqs[j][k],tmp_max_comp_dqs);
				tmp_total_max_comp_dqs-= max_comp_dqs[j][k];
				tmp_total_max_comp_dqs += tmp_max_comp_dqs;
				max_count--;
			}
		}	
		tmp_min_comp_dqs = tmp_total_min_comp_dqs>>3;
		avg_min_cg_comp_dqs[j] = 8-(tmp_min_comp_dqs>>3);
		avg_min_fg_comp_dqs[j] = 8-(tmp_min_comp_dqs&0x7);
		
		tmp_max_comp_dqs = tmp_total_max_comp_dqs>>3;
		avg_max_cg_comp_dqs[j] = 8+(tmp_max_comp_dqs>>3);
		avg_max_fg_comp_dqs[j] = 8+(tmp_max_comp_dqs&0x7);
		
	}
	//cal_debug("\n\r");
	//for (j = 0; j < 2; j++)	
	//		cal_debug("[%02X%02X%02X%02X]", avg_min_cg_comp_dqs[j],avg_min_fg_comp_dqs[j], avg_max_cg_comp_dqs[j],avg_max_fg_comp_dqs[j]);
	
	for (i=0 ; i < 2; i++)
	{
		unsigned int temp;
		coarse_dqs[i] = (avg_max_cg_comp_dqs[i] + avg_min_cg_comp_dqs[i])>>1; 
		temp = (((avg_max_cg_comp_dqs[i] + avg_min_cg_comp_dqs[i])%2)*4)  +  ((avg_max_fg_comp_dqs[i] + avg_min_fg_comp_dqs[i])>>1);
		if (temp >= 0x10)
		{
		   coarse_dqs[i] ++;
		   fine_dqs[i] = (temp-0x10) +0x8;
		}
		else
		{
			fine_dqs[i] = temp;
		}
	}

	reg = (coarse_dqs[1]<<12)|(fine_dqs[1]<<8)|(coarse_dqs[0]<<4)|fine_dqs[0];
	
#if defined (HWDLL_FIXED)
/* Read DLL HW delay */
{
	unsigned int sel_fine[2],sel_coarse[2];
	unsigned int sel_mst_coarse, sel_mst_fine;
	unsigned int avg_cg_dly[2],avg_fg_dly[2];
	
	hw_dll_reg = RALINK_REG(RALINK_MEMCTRL_BASE+0x20);
	sel_mst_coarse = (hw_dll_reg >> 8) & 0x0F;
	sel_mst_fine = (hw_dll_reg >> 4) & 0x0F;	
	for (j = 0; j < 2; j++)
	{	
		unsigned int cg_dly_adj, fg_dly_adj,sel_fine_tmp,sel_coarse_tmp;

		cg_dly_adj = coarse_dqs[j];
		fg_dly_adj = fine_dqs[j]; 	
		
		sel_fine_tmp = sel_mst_fine + fg_dly_adj - 8;
		sel_coarse_tmp = ((sel_mst_coarse + cg_dly_adj - 8) > DU_COARSE_WIDTH -1) ? DU_COARSE_WIDTH-1 : \
			  ((sel_mst_coarse + cg_dly_adj -8) < 0) ? 0 : sel_mst_coarse + cg_dly_adj -8;
		
		if (sel_fine_tmp > (DU_FINE_WIDTH-1)) {
			if (sel_coarse_tmp < (DU_COARSE_WIDTH-1)) {
				sel_fine[j] = sel_fine_tmp - C2F_RATIO;
				sel_coarse[j] = 	sel_coarse_tmp + 1;
			}
			else {
				sel_fine[j] = DU_FINE_WIDTH-1;
				sel_coarse[j] = 	sel_coarse_tmp;
			}
		}
		else if (sel_fine_tmp < 0){
			if (sel_coarse_tmp > 0) {
				sel_fine[j] = sel_fine_tmp + C2F_RATIO;
				sel_coarse[j] = 	sel_coarse_tmp - 1;
			}
			else {
				//saturate
				sel_fine[j] = 0;
				sel_coarse[j] = 	sel_coarse_tmp;
			}
		}
		else {
			sel_fine[j] = sel_fine_tmp;
			sel_coarse[j] = 	sel_coarse_tmp;
		}
	}
	reg_with_dll = (sel_coarse[1]<<28)|(sel_fine[1]<<24)|(sel_coarse[0]<<20)|(sel_fine[0]<<16);
	
#if defined(HWDLL_AVG)
	for (j = 0; j < 2; j++)
	{
		unsigned int avg;
		int min_coarse_dqs_dly,min_fine_dqs_dly; 
		min_coarse_dqs_dly = sel_mst_coarse - (8 - min_coarse_dqs[j]);
		min_fine_dqs_dly = sel_mst_fine - (8 -min_fine_dqs[j]);
		min_coarse_dqs_dly = (min_coarse_dqs_dly < 0) ? 0 : min_coarse_dqs_dly;
		min_fine_dqs_dly = (min_fine_dqs_dly < 0) ? 0 : min_fine_dqs_dly;
		
		
		avg_cg_dly[j] = ((min_coarse_dqs_dly<<1) + (sel_coarse[j]<<1))>>1;
		avg_cg_dly[j] = avg_cg_dly[j]&0x01 ? ((avg_cg_dly[j]>>1)+1) : (avg_cg_dly[j]>>1);
			
		avg_fg_dly[j] = ((min_fine_dqs_dly<<1) + (sel_fine[j]<<1))>>1;
		avg_fg_dly[j] = avg_fg_dly[j]&0x01 ? ((avg_fg_dly[j]>>1)+1) : (avg_fg_dly[j]>>1);
		
		g_min_coarse_dqs_dly[j] = min_coarse_dqs_dly;
		g_min_fine_dqs_dly[j] = min_fine_dqs_dly;
	}
	reg_avg = (avg_cg_dly[1]<<28)|(avg_fg_dly[1]<<24)|(avg_cg_dly[0]<<20)|(avg_fg_dly[0]<<16);
#endif

#if defined (HWDLL_MINSCAN)
{
	unsigned int min_scan_cg_dly[2], min_scan_fg_dly[2], adj_dly[2], reg_scan;
	
	RALINK_REG(RALINK_RGCTRL_BASE+0x108) = 0x01300;

	k=9583000;
	do {k--; }while(k>0);
		
	RALINK_REG(RALINK_MEMCTRL_BASE+0x64) = reg_with_dll;
	RALINK_REG(RALINK_MEMCTRL_BASE+0x68) |= (0x1<<4);
	
	for (j = 0; j < 2; j++)
	{
		min_scan_cg_dly[j] = 0;
		min_scan_fg_dly[j] = 0;
	
		do
		{	
				int diff_dly;
				for (nc_addr = 0xA0000000; nc_addr < (0xA0000000+DRAM_BUTTOM-NUM_OF_CACHELINE*32); nc_addr+=((DRAM_BUTTOM>>6)+1*0x400))
				{
					
					RALINK_REG(RALINK_MEMCTRL_BASE+0x64) = reg_with_dll;
					wmb();
					c_addr = (unsigned int*)((ulong)nc_addr & 0xDFFFFFFF);
					cal_memset(((unsigned char*)c_addr), 0x1F, NUM_OF_CACHELINE*32);
#if (NUM_OF_CACHELINE > 40)
					cal_patgen(nc_addr, NUM_OF_CACHELINE*8, 2);
#else			
					cal_memcpy(((unsigned char*)nc_addr), ((unsigned char*)cache_pat), NUM_OF_CACHELINE*32);
#endif			
					if (j == 0)
						reg_scan = (reg_with_dll&0xFF000000)|(min_scan_cg_dly[j]<<20)|(min_scan_fg_dly[j]<<16);
					else		
						reg_scan = (reg_with_dll&0x00FF0000)|(min_scan_cg_dly[j]<<28)|(min_scan_fg_dly[j]<<24);	
					
					RALINK_REG(RALINK_MEMCTRL_BASE+0x64) = reg_scan;
					wmb();		
					cal_invalidate_dcache_range(((unsigned char*)c_addr), ((unsigned char*)c_addr)+NUM_OF_CACHELINE*32);
					wmb();

					for (i = 0; i < NUM_OF_CACHELINE*8; i ++)
					{
						if (i % 8 ==0)
							pref_op(0, &c_addr[i]);
					}		
					for (i = 0; i < NUM_OF_CACHELINE*8; i ++)
					{
#if (NUM_OF_CACHELINE > 40)
						if (c_addr[i] != (ulong)nc_addr+i+2)
#else				
						if (c_addr[i] != cache_pat[i])
#endif				
						{
							goto MINSCAN_FAILED;
						}
					}
				}	
				diff_dly = (avg_cg_dly[j]*8 + avg_fg_dly[j])-(min_scan_cg_dly[j]*8+min_scan_fg_dly[j]);
				if (diff_dly < 0)
					cal_debug("diff_dly=%d\n",diff_dly);
					
				if (diff_dly < 6)
					adj_dly[j] = (avg_cg_dly[j]*8 + avg_fg_dly[j]) + (6 - diff_dly);
				else
					adj_dly[j] = (avg_cg_dly[j]*8 + avg_fg_dly[j]);

				break;
MINSCAN_FAILED:
				min_scan_fg_dly[j] ++;
				if (min_scan_fg_dly[j] > 8)
				{	
					min_scan_fg_dly[j] = 0;
					min_scan_cg_dly[j]++;
					if ((min_scan_cg_dly[j]*8+min_scan_fg_dly[j]) >= (avg_cg_dly[j]*8 + avg_fg_dly[j]))
					{
						if (j==0)
							adj_dly[0] = ((reg_with_dll>>20) &0x0F)*8 + ((reg_with_dll>>16) &0x0F);
						else
							adj_dly[1] = ((reg_with_dll>>28) &0x0F)*8 + ((reg_with_dll>>24) &0x0F);				
						break;
					}	
				}
		}while(1);		
	} /* dqs loop */
	{
		unsigned int tmp_cg_dly[2],tmp_fg_dly[2];
		for (j = 0; j < 2; j++)
		{
			if (adj_dly[j]==(avg_cg_dly[j]*8+avg_fg_dly[j]))
			{
				tmp_cg_dly[j] = avg_cg_dly[j];
				tmp_fg_dly[j] = avg_fg_dly[j];
			}
			else
			{
				tmp_cg_dly[j] = adj_dly[j]>>3;
				tmp_fg_dly[j] = adj_dly[j]&0x7;
			}
		}		
		reg_minscan = (tmp_cg_dly[1]<<28) | (tmp_fg_dly[1]<<24) | (tmp_cg_dly[0]<<20) | (tmp_fg_dly[0]<<16);
	}
}

#endif /* HWDLL_MINSCAN */

#if defined (HWDLL_LV)
	RALINK_REG(RALINK_RGCTRL_BASE+0x108) = 0x0f00;

	k=9583000;
	do {k--; }while(k>0);
#endif			

FINAL_SETUP:
#if (defined(HWDLL_AVG) && (!defined(HWDLL_MINSCAN)))
	RALINK_REG(RALINK_MEMCTRL_BASE+0x64) = (reg_avg&0xFFFF0000)|((reg_with_dll>>16)&0x0FFFF);		
#elif defined(HWDLL_MINSCAN)
	RALINK_REG(RALINK_MEMCTRL_BASE+0x64) = (reg_minscan&0xFFFF0000)|((reg_with_dll>>16)&0x0FFFF);		
#else	
	RALINK_REG(RALINK_MEMCTRL_BASE+0x64) = (reg_with_dll&0xFFFF0000)|(reg&0x0FFFF);
#endif		
	
	RALINK_REG(RALINK_MEMCTRL_BASE+0x68) |= (0x1<<4);
	cal_debug("\n\r");
	for (j = 0; j < 2; j++)	
			cal_debug("[%02X%02X%02X%02X]", avg_min_cg_comp_dqs[j],avg_min_fg_comp_dqs[j], avg_max_cg_comp_dqs[j],avg_max_fg_comp_dqs[j]);

	cal_debug("[%04X%02X%02X][%08X][00%04X%02X]\n", reg&0x0FFFF,\
						(g_min_coarse_dqs_dly[0]<<4)|g_min_coarse_dqs_dly[0], \
						(g_min_coarse_dqs_dly[1]<<4)|g_min_coarse_dqs_dly[1], \
						RALINK_REG(RALINK_MEMCTRL_BASE+0x64),
						(reg_avg&0xFFFF0000)>>16,
						(hw_dll_reg>>4)&0x0FF
						);
	cal_debug("DU Setting Cal Done\n");
	RALINK_REG(RALINK_MEMCTRL_BASE+0x48) = ddr_cfg2_reg;
#if defined(MT7628_FPGA_BOARD) || defined(MT7628_ASIC_BOARD)	
	RALINK_REG(RALINK_MEMCTRL_BASE+0x10) |= (0x1<<4);
#else
	RALINK_REG(RALINK_MEMCTRL_BASE+0x18) |= (0x1<<4);
#endif

#endif
}

EXIT:

	return ;
}
#endif /* #defined (CONFIG_DDR_CAL) */


/* Restore to default. */
int reset_to_default(void)
{
#ifndef CFG_ENV_IS_NOWHERE
	ulong addr, size;

	addr = CFG_ENV_ADDR;
	size = CFG_BOOTENV_SIZE;

	/* Erase U-Boot Env partition */
#if defined (CFG_ENV_IS_IN_SPI)
	printf("Erase 0x%08x size 0x%x\n", addr, size);
	raspi_erase((addr-CFG_FLASH_BASE), size);
#else
	printf("Erase 0x%08x size 0x%x\n", addr, size);
	flash_sect_protect(0, addr, addr+size-1);
	flash_sect_erase(addr, addr+size-1);
	flash_sect_protect(1, addr, addr+size-1);
#endif
#endif /* !CFG_ENV_IS_NOWHERE */
	return 0;
}
