/*
 * (C) Copyright 2002
 * Wolfgang Denk, DENX Software Engineering, <wd@denx.de>
 *
 * (C) Copyright 2002
 * Sysgo Real-Time Solutions, GmbH <www.elinos.com>
 * Marius Groeger <mgroeger@sysgo.de>
 *
 * (C) Copyright 2002
 * Sysgo Real-Time Solutions, GmbH <www.elinos.com>
 * Alex Zuepke <azu@sysgo.de>
 *
 * Copyright (C) 1999 2000 2001 Erik Mouw (J.A.K.Mouw@its.tudelft.nl)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <common.h>
#include "serial.h"
#include <rt_mmap.h>

/* this function does not need to know the cpu and bus clock after RT3883. the clock is fix at 40Mhz */
static void serial_setbrg (void)
{
	//DECLARE_GLOBAL_DATA_PTR;
	unsigned int clock_divisor = 0;

	//reset uart lite and uart full
	RALINK_REG(RALINK_RSTCTRL_REG) |= cpu_to_le32(RALINK_UART1_RST | RALINK_UART2_RST | RALINK_UART3_RST);
	RALINK_REG(RALINK_RSTCTRL_REG) &= ~cpu_to_le32(RALINK_UART1_RST | RALINK_UART2_RST | RALINK_UART3_RST);
#if 0
	u32 reg;
	reg = ra_inl(RT2880_GPIOMODE_REG);
	//UART2 as UART mode
	reg &= ~(0x3 << 3);
	//UART3 as UART mode
	reg &= ~(0x3 << 5);
	ra_outl(RT2880_GPIOMODE_REG, reg);
#endif

	/* RST Control change from W1C to W1W0 to reset, update 20080812 */
	//*(unsigned long *)(RALINK_SYSCTL_BASE + 0x0034) = 0;
	//clock_divisor = (CPU_CLOCK_RATE / SERIAL_CLOCK_DIVISOR / gd->baudrate);
	clock_divisor = (50 * 1000*1000/ SERIAL_CLOCK_DIVISOR / CONFIG_BAUDRATE);

	IER(CFG_RT2880_CONSOLE) = 0;		/* Disable for now */
	FCR(CFG_RT2880_CONSOLE) = 0;		/* No fifos enabled */

	/* set baud rate */
	LCR(CFG_RT2880_CONSOLE) = LCR_WLS0 | LCR_WLS1 | LCR_DLAB;
	DLL(CFG_RT2880_CONSOLE) = clock_divisor & 0xff;
	DLM(CFG_RT2880_CONSOLE) = clock_divisor >> 8;
	LCR(CFG_RT2880_CONSOLE) = LCR_WLS0 | LCR_WLS1;
}

/*
 * Initialise the serial port with the given baudrate. The settings
 * are always 8 data bits, no parity, 1 stop bit, no start bits.
 *
 */
int serial_init (void)
{
	serial_setbrg ();
	return (0);
}

/*
 * Output a single byte to the serial port.
 */
void serial_putc (const char c)
{
	/* wait for room in the tx FIFO on UART */
	while ((LSR(CFG_RT2880_CONSOLE) & LSR_TEMT) == 0);

	TBR(CFG_RT2880_CONSOLE) = c;

	/* If \n, also do \r */
	if (c == '\n')
		serial_putc ('\r');
}

/*
 * Read a single byte from the serial port. Returns 1 on success, 0
 * otherwise. When the function is succesfull, the character read is
 * written into its argument c.
 */
int serial_tstc (void)
{
	return LSR(CFG_RT2880_CONSOLE) & LSR_DR;
}

/*
 * Read a single byte from the serial port. Returns 1 on success, 0
 * otherwise. When the function is succesfull, the character read is
 * written into its argument c.
 */
int serial_getc (void)
{
	while (!(LSR(CFG_RT2880_CONSOLE) & LSR_DR));
	return (char) RBR(CFG_RT2880_CONSOLE) & 0xff;
}

void serial_puts (const char *s)
{
	while (*s) {
		serial_putc (*s++);
	}
}
