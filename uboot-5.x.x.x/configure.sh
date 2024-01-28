#!/bin/bash

# $1 - "Debug" DEBUG_BUILD=y
# $2 - "Use MIPS16 ASE" CONFIG_MIPS16_ASE=y
# $3 - "Boot Delay" CONFIG_BOOTDELAY=xxx
# $4 - "None", "LLLLW" "LLLLX" "LLLWW" "LLLXX" (LAN_WAN_PARTITION=y + RALINK_PVLAN_LLL**=y )
# $5 - "Enable All Ethernet PHY": "EPHY_LINK_UP=y

CONFIG=.config

if [ $1 == 'true' ]; then
	echo "DEBUG_BUILD=y" >> $CONFIG
fi

if [ $2 == 'true' ]; then
	echo "CONFIG_MIPS16_ASE=y" >> $CONFIG
fi

if [ $# -gt 2 ]; then
	echo "CONFIG_BOOTDELAY=$3" >> $CONFIG
fi

if [ $# -gt 3 ]; then
	case "$4" in
	 "LLLLW")
		echo "LAN_WAN_PARTITION=y" >> $CONFIG
		echo "RALINK_PVLAN_LLLLW=y" >> $CONFIG
		;;
	 "LLLLX")
		echo "LAN_WAN_PARTITION=y" >> $CONFIG
		echo "RALINK_PVLAN_LLLLX=y" >> $CONFIG
		;;
	 "LLLWW")
		echo "LAN_WAN_PARTITION=y" >> $CONFIG
		echo "RALINK_PVLAN_LLLWW=y" >> $CONFIG
		;;
	 "LLLXX")
		echo "LAN_WAN_PARTITION=y" >> $CONFIG
		echo "RALINK_PVLAN_LLLXX=y" >> $CONFIG
		;;
	 *)
		echo "# LAN_WAN_PARTITION is not set" >> $CONFIG
		;;
	esac
fi

if [ $5 == 'true' ]; then
	echo "EPHY_LINK_UP=y" >> $CONFIG
fi