#!/bin/sh
mkdir -p ../../../out/target/product/d10f/obj/BOOTLOADER_EMMC_OBJ
make PROJECT=msm8226 BOOTLOADER_OUT=../../../out/target/product/d10f/obj/BOOTLOADER_EMMC_OBJ EMMC_BOOT=1
