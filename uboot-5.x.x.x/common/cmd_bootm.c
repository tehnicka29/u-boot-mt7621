/*
 * (C) Copyright 2000-2002
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
 * Boot support
 */
#include <common.h>
#include <watchdog.h>
#include <command.h>
#include <image.h>
#include <malloc.h>
#include <rt_mmap.h>

#include <environment.h>
#include <asm/byteorder.h>

#if defined (CFG_ENV_IS_IN_SPI)
#include "spi_api.h"
#endif

#if defined (CONFIG_NETCONSOLE)
#include "net.h"
#endif

#ifdef CONFIG_GZIP
#include <zlib.h>
#endif /* CONFIG_GZIP */

#ifdef CONFIG_LZMA
#include <LzmaDecode.h>
#endif /* CONFIG_LZMA */

 /*cmd_boot.c*/
 extern int do_reset (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[]);

#if (CONFIG_COMMANDS & CFG_CMD_DATE) || defined(CONFIG_TIMESTAMP)
#include <rtc.h>
#endif

#ifdef CFG_HUSH_PARSER
#include <hush.h>
#endif

#ifdef CONFIG_SHOW_BOOT_PROGRESS
# include <status_led.h>
# define SHOW_BOOT_PROGRESS(arg)	show_boot_progress(arg)
#else
# define SHOW_BOOT_PROGRESS(arg)
#endif

#ifdef CFG_INIT_RAM_LOCK
#include <asm/cache.h>
#endif

#ifdef CONFIG_LOGBUFFER
#include <logbuff.h>
#endif

/*
 * Some systems (for example LWMON) have very short watchdog periods;
 * we must make sure to split long operations like memmove() or
 * crc32() into reasonable chunks.
 */
#if defined(CONFIG_HW_WATCHDOG) || defined(CONFIG_WATCHDOG)
# define CHUNKSZ (64 * 1024)
#endif


#if (CONFIG_COMMANDS & CFG_CMD_IMI)
#ifdef RT2880_U_BOOT_CMD_OPEN
static int image_info (unsigned long addr);
#endif
#endif

static void print_type (image_header_t *hdr);

#define CONFIG_NONE

/*
 *  Continue booting an OS image; caller already has:
 *  - copied image header to global variable `header'
 *  - checked header magic number, checksums (both header & image),
 *  - verified image architecture (PPC) and type (KERNEL or MULTI),
 *  - loaded (first part of) image to header load address,
 *  - disabled interrupts.
 */
typedef void boot_os_Fcn (cmd_tbl_t *cmdtp, int flag,
			  int	argc, char *argv[],
			  ulong	addr,		/* of image to boot */
			  ulong	*len_ptr,	/* multi-file image length table */
			  int	verify);	/* getenv("verify")[0] != 'n' */

#ifdef	DEBUG
extern int do_bdinfo ( cmd_tbl_t *cmdtp, int flag, int argc, char *argv[]);
#endif

extern boot_os_Fcn do_bootm_linux;

#ifdef CONFIG_SILENT_CONSOLE
static void fixup_silent_linux (void);
#endif

ulong load_addr = CFG_SPINAND_LOAD_ADDR;	/* Default Load Address */

static ulong lastAddr = 0;
static ulong lastData = 0;
static ulong lastLen = 0;

static inline void mips_cache_set(u32 v)
{
	asm volatile ("mtc0 %0, $16" : : "r" (v));
}

/* Check firmware image at addr
 * verify_data_crc:
 * -1 - get from "verify" environment variable
 *  0 - do not verify (enforced)
 *  1 - verify (enforced)
 *  2 - 1 + ignore last know check
 * @return:
 * 	> 0:	firmware good. length of firmware.
 * 	= 0:	not defined.
 * 	< 0:	invalid firmware.
 */
image_header_t header = { 0x0 };

int verify_kernel_image(ulong addr, ulong *pAddr, ulong *pData, ulong *pLen, int verify_data) {

	DECLARE_GLOBAL_DATA_PTR;
	uint32_t checksum, checksum2;
	ulong data, len;

	if (addr == lastAddr && ntohl(header.ih_magic) == IH_MAGIC) {
		if (memcmp((void*)addr, (void*)&header, sizeof(header)) == 0) {
			if (verify_data != 2) {
				printf("## skipping image check at %08lx\n", addr);
				goto skip;
			}
		}
	}

	printf("## Checking image at %08lx ...\n", addr);

	if (verify_data < 0) {
		char *s = getenv("verify");
		verify_data = (s && (*s == 'n')) ? 0 : 1;
	}

#if defined (CFG_ENV_IS_IN_SPI)
	if (addr >= CFG_FLASH_BASE)
		/* Copy header so we can blank CRC field for re-calculation */
		raspi_read((char *)&header, (addr - CFG_FLASH_BASE), sizeof(image_header_t));
	else
#endif
		memcpy(&header, (char *)addr, sizeof(image_header_t));

	SHOW_BOOT_PROGRESS(1);

	if (ntohl(header.ih_magic) != IH_MAGIC) {
		printf("Bad Magic Number - %08X\n", ntohl(header.ih_magic));
		SHOW_BOOT_PROGRESS(-1);
		return -1;
	}

	SHOW_BOOT_PROGRESS(2);

	checksum = ntohl(header.ih_hcrc);
	header.ih_hcrc = 0;

	checksum2 = crc32(0, (const unsigned char *)&header, sizeof(image_header_t));
	/* invalidate header magic */
	header.ih_magic = htonl(0xDEADC0DE);

	if (checksum2 != checksum) {
		printf("Bad Header Checksum: %08x expected, %08x calculated\n", checksum, checksum2);
		SHOW_BOOT_PROGRESS(-2);
		return -2;
	}

	header.ih_hcrc = checksum; /* restore checksum */

	print_image_hdr(&header);

	SHOW_BOOT_PROGRESS(3);

#if defined(__mips__)
	if (header.ih_arch != IH_CPU_MIPS)
#else
#error Unknown CPU type
#endif
	{
		printf("Unsupported Architecture 0x%x\n", header.ih_arch);
		SHOW_BOOT_PROGRESS(-3);
		return -3;
	}

	SHOW_BOOT_PROGRESS(4);

	switch (header.ih_type) {
#ifdef IH_TYPE_STANDALONE_SUPPORT
		case IH_TYPE_STANDALONE:
#endif
		case IH_TYPE_KERNEL:
			break;
		default:
			printf("Wrong Image Type 0x%x\n", header.ih_type);
			SHOW_BOOT_PROGRESS(-4);
			return -4;
	}

	SHOW_BOOT_PROGRESS(5);

	switch (header.ih_os) {
		case IH_OS_LINUX:
			break;
		default:
			printf("Unsupported OS Type 0x%x\n", header.ih_os);
			SHOW_BOOT_PROGRESS(-5);
			return -5;
	}

	SHOW_BOOT_PROGRESS(6);

	switch (header.ih_comp) {
		case IH_COMP_NONE: /* XIP */
		case IH_COMP_LZMA:
			break;
		default:
			printf("Unsupported Compression Method 0x%x\n", header.ih_comp);
			SHOW_BOOT_PROGRESS(-6);
			return -6;
	}
		
	SHOW_BOOT_PROGRESS(7);

	len = ntohl(header.ih_size);
	if (len > gd->bd->bi_flashsize - sizeof(image_header_t)) {
		printf("Bad Header Data Length - %d\n", len);
		SHOW_BOOT_PROGRESS(-7);
		return -7;
	}

	SHOW_BOOT_PROGRESS(8);

	data = addr + sizeof(image_header_t);
#if defined (CFG_ENV_IS_IN_SPI)
	if (addr >= CFG_FLASH_BASE) {
		ulong spi_load_addr = CFG_SPINAND_LOAD_ADDR;
		raspi_read((char *)spi_load_addr, data - CFG_FLASH_BASE, len);
		data = spi_load_addr;
	}
#endif

	if (verify_data > 0) {
		puts("   Verifying Checksum ... ");
		if (crc32(0, (const unsigned char *)data, len) != ntohl(header.ih_dcrc)) {
			printf("Bad Data CRC\n");
			SHOW_BOOT_PROGRESS(-8);
			return -8;
		}
		puts("OK\n");
	}

	/* restore header magic */
	header.ih_magic = htonl(IH_MAGIC);

	lastAddr = addr;
	lastData = data;
	lastLen = len;

skip:
	if (pAddr != NULL) *pAddr = lastAddr;
	if (pData != NULL) *pData = lastData;
	if (pLen != NULL) *pLen = lastLen;

	return (int)(lastLen);
}

/*
 return:
  0 - success
  1 - hang/reset
  2 - enter recovery if available
*/

int do_bootm (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	ulong	addr_src;
	ulong	addr, data, len;
	ulong	*len_ptr;
#if defined (CONFIG_GZIP) || defined (CONFIG_XZ)
	uint	unc_len = 0x800000;
#endif
	int	i;
	char *name;
	int	(*appl)(int, char *[]);
#ifdef CONFIG_UNCOMPRESS_TIME
	unsigned long long tAUncompress, tBUncompress;
#endif

	image_header_t *hdr = &header;

	//mips_cache_set(3);

	if (argc < 2) {
		addr_src = load_addr;
	} else {
		addr_src = simple_strtoul(argv[1], NULL, 16);
	}

	if (verify_kernel_image(addr_src, &addr, &data, &len, -1) <= 0)
		return 1;

	len_ptr = (ulong *)data;

	switch (hdr->ih_type) {
#ifdef IH_TYPE_STANDALONE_SUPPORT
	case IH_TYPE_STANDALONE:
		name = "Standalone Application";
		/* A second argument overwrites the load address */
		if (argc > 2) {
			hdr->ih_load = simple_strtoul(argv[2], NULL, 16);
		}
		break;
#endif
	case IH_TYPE_KERNEL:
		name = "Kernel Image";
		break;
	default: 
		printf ("Wrong Image Type for %s command\n", (cmdtp && cmdtp->name) ? cmdtp->name : __func__ );
		SHOW_BOOT_PROGRESS (-5);
		return 2; // TODO: blink alert leds?
	}
	SHOW_BOOT_PROGRESS (6);

	/*
	 * We have reached the point of no return: we are going to
	 * overwrite all exception vector code, so we cannot easily
	 * recover from any failures any more...
	 */

	//iflag = disable_interrupts();

#ifdef CONFIG_NETCONSOLE
	/*
	* Stop the ethernet stack if NetConsole could have
	* left it up
	*/
	puts("   Stopping network ... ");
	eth_halt();
	puts("OK\n");
#endif

	switch (hdr->ih_comp) {
#ifdef CONFIG_NONE
	case IH_COMP_NONE:
		if(ntohl(hdr->ih_load) == addr) {
			printf ("   XIP %s ... ", name);
		} else {
#if defined(CONFIG_HW_WATCHDOG) || defined(CONFIG_WATCHDOG)
			size_t l = len;
			void *to = (void *)ntohl(hdr->ih_load);
			void *from = (void *)data;

			printf ("   Loading %s ... ", name);

			while (l > 0) {
				size_t tail = (l > CHUNKSZ) ? CHUNKSZ : l;
				WATCHDOG_RESET();
				memmove (to, from, tail);
				to += tail;
				from += tail;
				l -= tail;
			}
#else	/* !(CONFIG_HW_WATCHDOG || CONFIG_WATCHDOG) */
			memmove ((void *) ntohl(hdr->ih_load), (uchar *)data, len);
#endif	/* CONFIG_HW_WATCHDOG || CONFIG_WATCHDOG */
		}
		break;
#endif
#ifdef CONFIG_GZIP
	case IH_COMP_GZIP:
		printf ("   Uncompressing %s ... ", name);
		if (gunzip ((void *)ntohl(hdr->ih_load), unc_len,
			    (uchar *)data, &len) != 0) {
			puts ("GUNZIP ERROR - must RESET board to recover\n");
			SHOW_BOOT_PROGRESS (-6);
			do_reset (cmdtp, flag, argc, argv);
		}
		break;
#endif
#ifdef CONFIG_LZMA
        case IH_COMP_LZMA:
                printf ("   Uncompressing %s ... ", name);

#ifdef CONFIG_UNCOMPRESS_TIME
                tBUncompress = get_ticks();
#endif
                unsigned int destLen = 0;
                watchdog_set(60); /* 60s is more than enough to decompress any valid iamge */
                i = lzmaBuffToBuffDecompress ((char*)ntohl(hdr->ih_load),
                                &destLen, (char *)data, len);
                if (i != LZMA_RESULT_OK) {
                        printf ("LZMA ERROR %d - must RESET board to recover\n", i);
                        SHOW_BOOT_PROGRESS (-6);
                        udelay(100000);
                        do_reset (cmdtp, flag, argc, argv);
                }
                watchdog_set(0); /* disable */
#ifdef CONFIG_UNCOMPRESS_TIME
                tAUncompress = get_ticks();
                tAUncompress = (uint32_t)((tAUncompress - tBUncompress) >> 10);
                printf("Uncompression time : %u/%u\n",(uint32_t)tAUncompress, (uint32_t)get_tbclk());
                printf("Uncompression length is %d\n",destLen);
#endif
                break;
#endif /* CONFIG_LZMA */
	default:
		/*
		if (iflag)
			enable_interrupts();
			*/
		printf ("Unimplemented compression type %d\n", hdr->ih_comp);
		SHOW_BOOT_PROGRESS (-7);
		return 2; // TODO: blink alert leds?
	}
	puts ("OK\n");
	SHOW_BOOT_PROGRESS (7);

	switch (hdr->ih_type) {
#ifdef IH_TYPE_STANDALONE_SUPPORT
	case IH_TYPE_STANDALONE:
		/*
		if (iflag)
			enable_interrupts();
			*/

		/* load (and uncompress), but don't start if "autostart"
		 * is set to "no"
		 */
#if 0
		if (((s = getenv("autostart")) != NULL) && (strcmp(s,"no") == 0)) {
			char buf[32];
			sprintf(buf, "%lX", len);
			setenv("filesize", buf);
			return 0;
		}
#endif
		appl = (int (*)(int, char *[]))ntohl(hdr->ih_ep);
		(*appl)(argc-1, &argv[1]);
		return 0;
#endif
	case IH_TYPE_KERNEL:
		break;
	default:
		/*
		if (iflag)
			enable_interrupts();
			*/
		printf ("Can't boot image type %d\n", hdr->ih_type);
		SHOW_BOOT_PROGRESS (-8);
		return 2; // TODO: blink alert leds?
	}
	SHOW_BOOT_PROGRESS (8);

	switch (hdr->ih_os) {
	default:			/* handled by (original) Linux case */
	case IH_OS_LINUX:
#ifdef CONFIG_SILENT_CONSOLE
	    fixup_silent_linux();
#endif
	    do_bootm_linux (cmdtp, flag, argc, argv, addr, len_ptr, 1 /*TODO: remove?*/ );
	    break;
	}

	SHOW_BOOT_PROGRESS (-9);
#ifdef DEBUG
	puts ("\n## Control returned to monitor - resetting...\n");
	do_reset (cmdtp, flag, argc, argv);
#endif
	// TODO: blink alert leds?
	return 1;
}

#ifdef RALINK_CMDLINE
U_BOOT_CMD(
 	bootm,	CFG_MAXARGS,	1,	do_bootm,
 	"bootm   - boot application image from memory\n",
 	"[addr [arg ...]]\n    - boot application image stored in memory\n"
 	"\tpassing arguments 'arg ...'; when booting a Linux kernel,\n"
 	"\t'arg' can be the address of an initrd image\n"
);
#endif

#ifdef CONFIG_SILENT_CONSOLE
static void
fixup_silent_linux ()
{
	DECLARE_GLOBAL_DATA_PTR;
	char buf[256], *start, *end;
	char *cmdline = getenv ("bootargs");

	/* Only fix cmdline when requested */
	if (!(gd->flags & GD_FLG_SILENT))
		return;

	debug ("before silent fix-up: %s\n", cmdline);
	if (cmdline) {
		if ((start = strstr (cmdline, "console=")) != NULL) {
			end = strchr (start, ' ');
			strncpy (buf, cmdline, (start - cmdline + 8));
			if (end)
				strcpy (buf + (start - cmdline + 8), end);
			else
				buf[start - cmdline + 8] = '\0';
		} else {
			strcpy (buf, cmdline);
			strcat (buf, " console=");
		}
	} else {
		strcpy (buf, "console=");
	}

	setenv ("bootargs", buf);
	debug ("after silent fix-up: %s\n", buf);
}
#endif /* CONFIG_SILENT_CONSOLE */


#if (CONFIG_COMMANDS & CFG_CMD_BOOTD)
int do_bootd (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	int rcode = 0;
#ifndef CFG_HUSH_PARSER
	if (run_command (getenv ("bootcmd"), flag) < 0) rcode = 1;
#else
	if (parse_string_outer(getenv("bootcmd"),
		FLAG_PARSE_SEMICOLON | FLAG_EXIT_FROM_LOOP) != 0 ) rcode = 1;
#endif
	return rcode;
}

U_BOOT_CMD(
 	boot,	1,	1,	do_bootd,
 	"boot    - boot default, i.e., run 'bootcmd'\n",
	NULL
);

/* keep old command name "bootd" for backward compatibility */
U_BOOT_CMD(
 	bootd, 1,	1,	do_bootd,
 	"bootd   - boot default, i.e., run 'bootcmd'\n",
	NULL
);

#endif

#ifdef RALINK_CMDLINE
#if (CONFIG_COMMANDS & CFG_CMD_IMI)
int do_iminfo ( cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	int	arg;
	ulong	addr;
	int     rcode=0;

	if (argc < 2) {
		return image_info (load_addr);
	}

	for (arg=1; arg <argc; ++arg) {
		addr = simple_strtoul(argv[arg], NULL, 16);
		if (image_info (addr) != 0) rcode = 1;
	}

	return rcode;
}

U_BOOT_CMD(
	iminfo,	CFG_MAXARGS,	1,	do_iminfo,
	"iminfo  - print header information for application image\n",
	"addr [addr ...]\n"
	"    - print header information for application image starting at\n"
	"      address 'addr' in memory; this includes verification of the\n"
	"      image contents (magic number, header and payload checksums)\n"
);
#endif	/* CFG_CMD_IMI */

static int image_info(ulong addr)
{
	image_header_t header;
	ulong checksum, data, len;

	if (ntohl(((image_header_t *)addr)->ih_magic) != IH_MAGIC) {
		return -1;
	}

	memcpy(&header, (void *)addr, sizeof(image_header_t));
	checksum = ntohl(header.ih_hcrc);
	header.ih_hcrc = 0;

	if (crc32(0, (char *)&header, sizeof(image_header_t)) != checksum) {
		return -2;
	}

	printf("Image at %08X:\n", addr);
	print_image_hdr((image_header_t *)addr);

	data = addr + sizeof(image_header_t);
	len = ntohl(header.ih_size);

	puts("   Verifying Checksum ... ");
	if (crc32(0, (char *)(addr + sizeof(image_header_t)), ntohl(header.ih_size)) != ntohl(header.ih_dcrc)) {
		puts("   Bad Data CRC\n");
		return -3;
	}
	puts("OK\n");
	return 0;
}
#endif /*RALINK_CMDLINE */

void print_image_hdr (image_header_t *hdr)
{
	char namebuf[IH_NMLEN + 1];
#if (CONFIG_COMMANDS & CFG_CMD_DATE) || defined(CONFIG_TIMESTAMP)
	time_t timestamp = (time_t)ntohl(hdr->ih_time);
	struct rtc_time tm;
#endif
	uint8_t *p_name = namebuf;
	int i_name = IH_NMLEN;

	memcpy(namebuf, hdr->ih_name, IH_NMLEN);
	namebuf[IH_NMLEN] = '\0';

	/* check ASUS specific header */
	if (p_name[0] < 0x20 || p_name[0] > 0x7e) {
		p_name += 4;
		i_name -= 4;
	}

	printf ("   Image Name:   %.*s\n", i_name, p_name);
#if (CONFIG_COMMANDS & CFG_CMD_DATE) || defined(CONFIG_TIMESTAMP)
	to_tm (timestamp, &tm);
	printf ("   Created:      %4d-%02d-%02d  %2d:%02d:%02d UTC\n",
		tm.tm_year, tm.tm_mon, tm.tm_mday,
		tm.tm_hour, tm.tm_min, tm.tm_sec);
#endif	/* CFG_CMD_DATE, CONFIG_TIMESTAMP */

	puts ("   Image Type:   "); print_type(hdr);
	printf ("\n   Data Size:    %d Bytes = ", ntohl(hdr->ih_size));
	print_size (ntohl(hdr->ih_size), "\n");
	printf ("   Load Address: %08x\n"
		"   Entry Point:  %08x\n",
		 ntohl(hdr->ih_load), ntohl(hdr->ih_ep));
}

static void print_type(image_header_t *hdr)
{
	char *os, *arch, *type, *comp;

	switch (hdr->ih_os) {
	case IH_OS_INVALID:	os = "Invalid OS";		break;
	case IH_OS_NETBSD:	os = "NetBSD";			break;
	case IH_OS_LINUX:	os = "Linux";			break;
	case IH_OS_VXWORKS:	os = "VxWorks";			break;
	case IH_OS_QNX:		os = "QNX";				break;
	case IH_OS_U_BOOT:	os = "U-Boot";			break;
	case IH_OS_RTEMS:	os = "RTEMS";			break;
	default:			os = "Unknown OS";		break;
	}

	switch (hdr->ih_arch) {
	case IH_CPU_INVALID:	arch = "Invalid CPU";		break;
	case IH_CPU_ALPHA:	arch = "Alpha";			break;
	case IH_CPU_ARM:	arch = "ARM";			break;
	case IH_CPU_I386:	arch = "Intel x86";		break;
	case IH_CPU_IA64:	arch = "IA64";			break;
	case IH_CPU_MIPS:	arch = "MIPS";			break;
	case IH_CPU_MIPS64:	arch = "MIPS 64 Bit";		break;
	case IH_CPU_PPC:	arch = "PowerPC";		break;
	case IH_CPU_S390:	arch = "IBM S390";		break;
	case IH_CPU_SH:		arch = "SuperH";		break;
	case IH_CPU_SPARC:	arch = "SPARC";			break;
	case IH_CPU_SPARC64:	arch = "SPARC 64 Bit";		break;
	case IH_CPU_M68K:	arch = "M68K"; 			break;
	case IH_CPU_MICROBLAZE:	arch = "Microblaze"; 		break;
	default:		arch = "Unknown Architecture";	break;
	}

	switch (hdr->ih_type) {
	case IH_TYPE_INVALID:	type = "Invalid Image";		break;
	case IH_TYPE_STANDALONE:type = "Standalone Program";	break;
	case IH_TYPE_KERNEL:	type = "Kernel Image";		break;
	case IH_TYPE_RAMDISK:	type = "RAMDisk Image";		break;
	case IH_TYPE_MULTI:		type = "Multi-File Image";	break;
	case IH_TYPE_FIRMWARE:	type = "Firmware";		break;
	case IH_TYPE_SCRIPT:	type = "Script";		break;
	default:				type = "Unknown Image";	break;
	}

	switch (hdr->ih_comp) {
	case IH_COMP_NONE:	comp = "uncompressed";		break;
	case IH_COMP_GZIP:	comp = "gzip compressed";	break;
	case IH_COMP_BZIP2:	comp = "bzip2 compressed";	break;
	case IH_COMP_LZMA:	comp = "lzma compressed";	break;
	case IH_COMP_XZ:	comp = "xz compressed";		break;
	default:			comp = "unknown compression";	break;
	}

	printf ("%s %s %s (%s)", arch, os, type, comp);
}

#ifdef CONFIG_GZIP
#define	ZALLOC_ALIGNMENT	16

static void *zalloc(void *x, unsigned items, unsigned size)
{
	void *p;

	size *= items;
	size = (size + ZALLOC_ALIGNMENT - 1) & ~(ZALLOC_ALIGNMENT - 1);

	p = malloc (size);

	return (p);
}

static void zfree(void *x, void *addr, unsigned nb)
{
	free (addr);
}


#define HEAD_CRC	2
#define EXTRA_FIELD	4
#define ORIG_NAME	8
#define COMMENT		0x10
#define RESERVED	0xe0

#define DEFLATED	8

int gunzip(void *dst, int dstlen, unsigned char *src, unsigned long *lenp)
{
	z_stream s;
	int r, i, flags;

	/* skip header */
	i = 10;
	flags = src[3];
	if (src[2] != DEFLATED || (flags & RESERVED) != 0) {
		puts ("Error: Bad gzipped data\n");
		return (-1);
	}
	if ((flags & EXTRA_FIELD) != 0)
		i = 12 + src[10] + (src[11] << 8);
	if ((flags & ORIG_NAME) != 0)
		while (src[i++] != 0)
			;
	if ((flags & COMMENT) != 0)
		while (src[i++] != 0)
			;
	if ((flags & HEAD_CRC) != 0)
		i += 2;
	if (i >= *lenp) {
		puts ("Error: gunzip out of data in header\n");
		return (-1);
	}

	s.zalloc = zalloc;
	s.zfree = zfree;
#if defined(CONFIG_HW_WATCHDOG) || defined(CONFIG_WATCHDOG)
	s.outcb = (cb_func)WATCHDOG_RESET;
#else
	s.outcb = Z_NULL;
#endif	/* CONFIG_HW_WATCHDOG */

	r = inflateInit2(&s, -MAX_WBITS);
	if (r != Z_OK) {
		printf ("Error: inflateInit2() returned %d\n", r);
		return (-1);
	}
	s.next_in = src + i;
	s.avail_in = *lenp - i;
	s.next_out = dst;
	s.avail_out = dstlen;
	r = inflate(&s, Z_FINISH);
	if (r != Z_OK && r != Z_STREAM_END) {
		printf ("Error: inflate() returned %d\n", r);
		return (-1);
	}
	*lenp = s.next_out - (unsigned char *) dst;
	inflateEnd(&s);

	return (0);
}
#endif // CONFIG_GZIP //
