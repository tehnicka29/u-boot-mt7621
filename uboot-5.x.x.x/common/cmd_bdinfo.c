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
 * Boot support
 */
#include <common.h>
#include <command.h>
#include <net.h>		/* for print_IPaddr */

#if (CONFIG_COMMANDS & CFG_CMD_BDI)

static void print_num( const char *name, ulong value )
{
	printf( "%-12s= 0x%08lX\n", name, value );
}

#if 0
static void print_str( const char *name, const char *str )
{
	printf( "%-12s= %6s MHz\n", name, str );
}
#endif

static int do_bdinfo ( cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	DECLARE_GLOBAL_DATA_PTR;

	int i;
	bd_t *bd = gd->bd;

	print_num ("boot_params",	(ulong)bd->bi_boot_params);
	print_num ("memstart",		(ulong)bd->bi_memstart);
	print_num ("memsize",		(ulong)bd->bi_memsize);
	print_num ("flashstart",	(ulong)bd->bi_flashstart);
	print_num ("flashsize",		(ulong)bd->bi_flashsize);
	print_num ("flashoffset",	(ulong)bd->bi_flashoffset);

	puts ("ethaddr     =");
	for (i=0; i<6; ++i) {
		printf ("%c%02X", i ? ':' : ' ', bd->bi_enetaddr[i]);
	}
	puts ("\nip_addr     = ");
	print_IPaddr (bd->bi_ip_addr);
	printf ("\nbaudrate    = %d bps\n", bd->bi_baudrate);

	return 0;
}

U_BOOT_CMD(
	bdinfo,	1,	1,	do_bdinfo,
	"bdinfo  - print Board Info structure\n",
	NULL
);

#endif /* CONFIG_COMMANDS & CFG_CMD_BDI */
