#!/bin/bash

source /opt/yocto/2.4.2/environment-setup-armv7at2hf-neon-fslc-linux-gnueabi

BOARD=$1
SC100_ROOTFS_PATH=/home/user/projects/rootfs
SC200_ROOTFS_PATH=/home/user/projects/rootfs
TFTP_PATH=/home/user/projects/tftp

echo "Board type is" $BOARD


if [ "sc100" == "$1" ]; then
	IMX6TYPE="sc100_defconfig"
	IMX6DTB="imx6ul-sc100.dtb"
	ROOTFS_PATH=$SC100_ROOTFS_PATH
elif [ "sc200" == "$1" ]; then
	IMX6TYPE="sc200_defconfig"
	IMX6DTB="imx6dl-sc200-ldo.dtb"
	ROOTFS_PATH=$SC200_ROOTFS_PATH
elif [ "playgo" == "$1" ]; then
	IMX6TYPE="playgo_defconfig"
	IMX6DTB="imx6ul-playgo.dtb"
	ROOTFS_PATH=$SC200_ROOTFS_PATH
elif [ "evk" == "$1" ]; then
	IMX6TYPE="imx_v7_defconfig" 
	IMX6DTB="imx6ul-14x14-evk.dtb"
	ROOTFS_PATH=$SC100_ROOTFS_PATH
else
	echo "Clean Build"
	make mrproper
	exit 0
fi

if [ "menuconfig" == "$2" ]; then
	echo "Menu Config"
	make $IMX6TYPE
	make menuconfig
elif [ "build" == "$2" ]; then
	echo "Config file is " $IMX6TYPE
	echo "Device trees is " $IMX6DTB

	# make $IMX6TYPE
	make -j4 zImage
	make -j4 modules
	make -j4 $IMX6DTB

elif [ "install" == "$2" ]; then
	ZIMAGE="zImage-$1"
	echo "Copying zImage to  " $ZIMAGE
	cp arch/arm/boot/zImage $TFTP_PATH/$ZIMAGE
	cp arch/arm/boot/dts/$IMX6DTB $TFTP_PATH
	sudo make ARCH=arm modules_install INSTALL_MOD_PATH=$ROOTFS_PATH
fi
