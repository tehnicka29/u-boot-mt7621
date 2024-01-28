/*
 * (C) Copyright 2001-2004
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
#include <net.h>

#if (CONFIG_COMMANDS & CFG_CMD_NET)

#if defined (CFG_ENV_IS_IN_SPI)
#include "spi_api.h"
#endif

extern int rt2880_eth_initialize(bd_t *bis);

/*static struct eth_device *eth_devices, *eth_current;*/

static struct eth_device *eth_devices;
struct eth_device *eth_current;

void eth_parse_enetaddr(const char *addr, uchar *enetaddr)
{
	char *end;
	int i;

	for (i = 0; i < 6; ++i) {
		enetaddr[i] = addr ? simple_strtoul(addr, &end, 16) : 0;
		if (addr)
			addr = (*end) ? end + 1 : end;
	}
}

#ifdef CONFIG_NET_MULTI
/*struct eth_device *eth_get_dev(void)
{
	return eth_current;
}*/

int eth_get_dev_index (void)
{
	struct eth_device *dev;
	int num = 0;

	if (!eth_devices) {
		return (-1);
	}

	for (dev = eth_devices; dev; dev = dev->next) {
		if (dev == eth_current)
			break;
		++num;
	}

	if (dev) {
		return (num);
	}

	return (0);
}
#endif

int eth_register(struct eth_device* dev)
{
	struct eth_device *d;

	if (!eth_devices) {
		eth_current = eth_devices = dev;
#ifdef CONFIG_NET_MULTI
		/* update current ethernet name */
		{
			char *act = getenv("ethact");
			if (act == NULL || strcmp(act, eth_current->name) != 0)
				setenv("ethact", eth_current->name);
		}
#endif
	} else {
		for (d=eth_devices; d->next!=eth_devices; d=d->next);
		d->next = dev;
	}

	dev->state = ETH_STATE_INIT;
	dev->next  = eth_devices;

	return 0;
}

void eth_preinit(void)
{
	eth_devices = NULL;
	eth_current = NULL;
}

int eth_initialize(bd_t *bis)
{
	unsigned char rt2880_gmac1_mac[6];
	int eth_number = 0, regValue=0;

	eth_preinit();

#if defined(CONFIG_RT2880_ETH)
	rt2880_eth_initialize(bis);
#endif

	if (!eth_devices) {
		puts ("No ethernet found.\n");
	} else {
		struct eth_device *dev = eth_devices;
		char *ethprime = getenv ("ethprime");
		const unsigned char empty_mac[2][6] = { { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } };

		do {
			if (eth_number)
				puts (", ");

			if (ethprime && strcmp (dev->name, ethprime) == 0) {
				eth_current = dev;
				puts (" [PRIME]");
			}

			//get Ethernet mac address from flash
#if defined (CFG_ENV_IS_IN_SPI)
#ifdef GMAC0_OFFSET
			raspi_read(rt2880_gmac1_mac, GMAC0_OFFSET, 6);
#else
#error		"GMAC0_OFFSET is not set"
			#define GMAC0_OFFSET	0xE000
			raspi_read( rt2880_gmac1_mac, CFG_RADIO_OFFSET + GMAC0_OFFSET, 6 );
#endif
#else
#error		"Unsupported configuration"
			//memmove(rt2880_gmac1_mac, CFG_RADIO_ADDR + GMAC0_OFFSET, 6);
#endif
			//if flash is empty, use default mac address
			if (memcmp(rt2880_gmac1_mac, empty_mac[0], 6) == 0 || memcmp(rt2880_gmac1_mac, empty_mac[1], 6) == 0)
				eth_parse_enetaddr(CONFIG_ETHADDR, rt2880_gmac1_mac);

			memcpy(dev->enetaddr, rt2880_gmac1_mac, 6);

			eth_number++;
			dev = dev->next;
		} while(dev != eth_devices);

#ifdef CONFIG_NET_MULTI
		/* update current ethernet name */
		if (eth_current) {
			char *act = getenv("ethact");
			if (act == NULL || strcmp(act, eth_current->name) != 0)
				setenv("ethact", eth_current->name);
		} else
			setenv("ethact", NULL);
#endif
		//printf("\n eth_current->name = %s\n",eth_current->name);
		printf("\n");
	}

	return eth_number;
}

#ifdef CONFIG_NET_MULTI
void eth_set_enetaddr(int num, char *addr) {
	struct eth_device *dev;
	unsigned char enetaddr[6];
	char *end;
	int i;

	debug ("eth_set_enetaddr(num=%d, addr=%s)\n", num, addr);

	if (!eth_devices)
		return;

	for (i=0; i<6; i++) {
		enetaddr[i] = addr ? simple_strtoul(addr, &end, 16) : 0;
		if (addr)
			addr = (*end) ? end+1 : end;
	}

	dev = eth_devices;
	while(num-- > 0) {
		dev = dev->next;

		if (dev == eth_devices)
			return;
	}

	debug ( "Setting new HW address on %s\n"
		"New Address is             %02X:%02X:%02X:%02X:%02X:%02X\n",
		dev->name,
		dev->enetaddr[0], dev->enetaddr[1],
		dev->enetaddr[2], dev->enetaddr[3],
		dev->enetaddr[4], dev->enetaddr[5]);

	memcpy(dev->enetaddr, enetaddr, 6);
}
#endif
int eth_init(bd_t *bis)
{
	struct eth_device* old_current;

	if (!eth_current)
		return 0;

	old_current = eth_current;
	do {
		debug ("Trying %s\n", eth_current->name);

		if (eth_current->init(eth_current, bis)) {
			eth_current->state = ETH_STATE_ACTIVE;
			printf("\n ETH_STATE_ACTIVE!! \n");
			return 1;
		}
		printf  ("FAIL\n");
        //kaiker
		return (-1);

		eth_try_another(0);
	} while (old_current != eth_current);

	return 0;
}

void eth_halt(void)
{
	if (!eth_current)
		return;

	eth_current->halt(eth_current);

	eth_current->state = ETH_STATE_PASSIVE;
}

int eth_send(volatile void *packet, int length)
{
	if (!eth_current)
		return -1;

	return eth_current->send(eth_current, packet, length);
}

int eth_rx(void)
{
	if (!eth_current)
		return -1;

	return eth_current->recv(eth_current);
}

void eth_try_another(int first_restart)
{
	static struct eth_device *first_failed = NULL;

	if (!eth_current)
		return;

	if (first_restart) {
		first_failed = eth_current;
	}

	eth_current = eth_current->next;

#ifdef CONFIG_NET_MULTI
	/* update current ethernet name */
	{
		char *act = getenv("ethact");
		if (act == NULL || strcmp(act, eth_current->name) != 0)
			setenv("ethact", eth_current->name);
	}

	if (first_failed == eth_current) {
		NetRestartWrap = 1;
	}
#endif
}

#ifdef CONFIG_NET_MULTI
void eth_set_current(void)
{
	char *act;
	struct eth_device* old_current;

	if (!eth_current)	/* XXX no current */
		return;

	act = getenv("ethact");
	if (act != NULL) {
		old_current = eth_current;
		do {
			if (strcmp(eth_current->name, act) == 0)
				return;
			eth_current = eth_current->next;
		} while (old_current != eth_current);
	}

	setenv("ethact", eth_current->name);
}
#endif

#if defined(CONFIG_NET_MULTI)
char *eth_get_name (void)
{
	return (eth_current ? eth_current->name : "unknown");
}
#endif
#endif
