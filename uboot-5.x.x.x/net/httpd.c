/*
 *	Copyright 1994, 1995, 2000 Neil Russell.
 *	(See License)
 *	Copyright 2000, 2001 DENX Software Engineering, Wolfgang Denk, wd@denx.de
 */

#include <common.h>
#include <command.h>
#include <net.h>
#include <gpio.h>
#include <asm/byteorder.h>

#if defined(CFG_CMD_HTTPD)
#include "httpd.h"

#include "spi_api.h"

#include "../httpd/uipopt.h"
#include "../httpd/uip.h"
#include "../httpd/uip_arp.h"

static int arptimer;

void HttpdHandler(void){
	int i;
	struct uip_eth_hdr *eth_hdr = (struct uip_eth_hdr *)uip_buf;
	if (uip_len == 0) {
		for(i = 0; i < UIP_CONNS; i++){
			uip_periodic(i);
			if(uip_len > 0){
				uip_arp_out();
				NetSendHttpd();
			}
		}

		if(++arptimer == 20){
			uip_arp_timer();
			arptimer = 0;
		}
	} else {
		if (eth_hdr->type == htons(UIP_ETHTYPE_IP)) {
			uip_arp_ipin();
			uip_input();
			if(uip_len > 0){
				uip_arp_out();
				NetSendHttpd();
			}
		} else if (eth_hdr->type == htons(UIP_ETHTYPE_ARP)) {
			uip_arp_arpin();
			if(uip_len > 0){
				NetSendHttpd();
			}
		}
	}
}

// start http daemon
void HttpdStart(void){
	arptimer = 0;
	uip_init();
	httpd_init();
}

static int flash_erase_write(char *buf, long offs, unsigned long count) {
	int result;
#if defined (CFG_ENV_IS_IN_SPI)
	result = raspi_erase_write(buf, offs, count);
#else
	unsigned long e_end = CFG_FLASH_BASE + offs + count - 1;
	if (get_addr_boundary(&e_end) != 0)
		return -1;

	flash_sect_erase(CFG_FLASH_BASE + offs, e_end);
	result = flash_write(buf, CFG_FLASH_BASE + offs, count);
#endif
	return result;
}

extern unsigned char *webfailsafe_data_pointer_base;

int do_http_upgrade(const ulong size, const int upgrade_type) {
	char *data = webfailsafe_data_pointer_base;
	if (upgrade_type == WEBFAILSAFE_UPGRADE_TYPE_UBOOT) {
		// TODO: check signature (AT LEAST - for correct interrupt vector table?)
		printf("\n\n****************************\n*     U-BOOT UPGRADING     *\n* DO NOT POWER OFF DEVICE! *\n****************************\n\n");
		return(flash_erase_write(data, CFG_BOOTLOADER_OFFSET, UBOOT_FILE_SIZE_MAX ) );
	} else if (upgrade_type == WEBFAILSAFE_UPGRADE_TYPE_FIRMWARE) {
		// TODO: check signature (correct image header, data must be larger than header size?)
		printf("\n\n****************************\n*    FIRMWARE UPGRADING    *\n* DO NOT POWER OFF DEVICE! *\n****************************\n\n");
		return(flash_erase_write(data, CFG_KERNEL_OFFSET, size));
	} else if (upgrade_type == WEBFAILSAFE_UPGRADE_TYPE_RADIO) {
		printf("\n\n****************************\n*    RADIO  UPGRADING    *\n* DO NOT POWER OFF DEVICE! *\n****************************\n\n");
		return( flash_erase_write(data, CFG_RADIO_OFFSET, CFG_RADIO_SIZE ) );
	} else if (upgrade_type == WEBFAILSAFE_UPGRADE_TYPE_CONFIG) {
		// TODO: check signature (correct mac, etc.)
		printf( "\n\n****************************\n*    CONFIG  UPGRADING    *\n* DO NOT POWER OFF DEVICE! *\n****************************\n\n" );
		return ( flash_erase_write(data, CFG_CONFIG_OFFSET, CFG_CONFIG_SIZE));
	} else if (upgrade_type == WEBFAILSAFE_UPGRADE_TYPE_TPLINK) {
		// TODO: check signature (correct partition table, etc.)
		printf( "\n\n****************************\n*    TPLINK  UPGRADING    *\n* DO NOT POWER OFF DEVICE! *\n****************************\n\n" );
		return (flash_erase_write(data, CFG_TPLINK_OFFSET, CFG_TPLINK_SIZE ));
	} else if (upgrade_type == WEBFAILSAFE_UPGRADE_TYPE_INITRAMFS) {
		// TODO: check signature (correct image header, data must be equal to header size?)
		char *argv[2], buf[12];
		printf( "\n\n****************************\n*   INITRAMFS UPLOADING   *\n* DO NOT POWER OFF DEVICE! *\n****************************\n\n" );
		LED_ALERT_OFF();
		sprintf(buf, "0x%08X", data);
		argv[0] = (char*)__func__; // or "bootm"
		argv[1] = buf;
		do_bootm( NULL, 0, 2, argv );
	} else if (upgrade_type == WEBFAILSAFE_UPGRADE_TYPE_ROOTFS_RESET) {
		printf("\n\n****************************\n*     RESETTING ROOTFS    *\n* DO NOT POWER OFF DEVICE! *\n****************************\n\n");
		return rootfs_func(0 /* erase/reset */);
	}
	return(-1);
}

// info about current progress of failsafe mode
int do_http_progress(const int state){
	unsigned char i = 0;

	/* toggle LED's here */
	switch(state){
		case WEBFAILSAFE_PROGRESS_START:

			// blink LED fast a few times
			for (i = 0; i < 10; ++i) {
				LED_WPS_ON();
				milisecdelay(50);
				LED_WPS_OFF();
				milisecdelay(50);
			}

			printf("HTTP server is ready!\n\n");
			break;

		case WEBFAILSAFE_PROGRESS_TIMEOUT:
			//printf("Waiting for request...\n");
			break;

		case WEBFAILSAFE_PROGRESS_UPLOAD_READY:
			printf("HTTP upload is done! Upgrading...\n");
			break;

		case WEBFAILSAFE_PROGRESS_UPGRADE_READY:
			printf("HTTP ugrade is done! Rebooting...\n\n");
			break;

		case WEBFAILSAFE_PROGRESS_UPGRADE_FAILED:
			printf("*** ERROR: HTTP ugrade failed!\n\n");

			// blink LED fast for 4 sec
			for(i = 0; i < 40; ++i){
				LED_ALERT_ON();
				milisecdelay(50);
				LED_ALERT_OFF();
				milisecdelay(50);
			}

			// wait 1 sec
			milisecdelay(1000);

			break;
	}

	return(0);
}
#endif /* CFG_CMD_HTTPD */
